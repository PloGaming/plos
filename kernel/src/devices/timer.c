#include <common/logging.h>
#include <devices/timer.h>
#include <scheduling/scheduler.h>
#include <scheduling/task.h>
#include <drivers/lapic.h>
#include <drivers/portsIO.h>
#include <interrupts/isr.h>
#include <stdint.h>

static uint64_t system_ticks = 0;
static uint64_t lapic_ticks_per_ms = 0;
static uint64_t tsc_freq_hz = 0;
static uint64_t tsc_boot_time = 0;

/**
 * @brief Prepare the PIT to count ms milliseconds
 * 
 * @param ms How many milliseconds should the PIT count
 */
static void pit_prepare_sleep(uint64_t ms)
{
    uint16_t count_num = (PIT_BASE_FREQ / 1000) * ms;

    outb(PIT_CMD_PORT, 0x30);
    outb(PIT_DATA_PORT, count_num & 0xFF); // Set the low byte
    outb(PIT_DATA_PORT, (count_num >> 8) & 0xFF); // Set the upper byte
}

/**
 * @brief Read the current value of the PIT timer
 * 
 * @return uint16_t the current value of the PIT
 */
static uint16_t pit_read_count(void)
{
    outb(PIT_CMD_PORT, 0x00);
    uint8_t low = inb(PIT_DATA_PORT);
    uint8_t high = inb(PIT_DATA_PORT);
    return low | (high << 8);
}

/**
 * @brief Reads the TSC current value
 * Hopefully it's the invariant version and not the before 2008 one : )
 * @return uint64_t 
 */
static inline uint64_t rdtsc(void) 
{
    uint32_t lo, hi; 
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

/**
 * @brief Initializes and calibrates both the LAPIC timer and TSC
 */
void timer_init(void)
{
    log_line(LOG_DEBUG, "%s: Calibrating LAPIC timer with PIT", __FUNCTION__);

    // So it counts 1 tick every 16 (otherwise it's too quick)
    lapic_write(LAPIC_DIVIDE_REG, 0x3);
    // We initialize it at the maximum value 2^32 - 1
    lapic_write(LAPIC_INITIAL_COUNT_REG, 0xFFFFFFFF);

    pit_prepare_sleep(0xFFFF);

    // Starting "snapshot" of both timers
    uint32_t start_lapic_timer = lapic_read(LAPIC_CURRENT_COUNT_REG);
    uint64_t start_tsc_timer = rdtsc();

    uint16_t start_pit, current;
    start_pit = current = pit_read_count();

    // 10ms of calibration
    uint16_t ticks_to_wait = 11931;
    while(1)
    {
        uint16_t current = pit_read_count();
        uint16_t delta = start_pit - current;
        if (delta >= ticks_to_wait) {
            break;
        }
    }

    // Final "snapshot" of both timers
    lapic_write(LAPIC_LVT_TIMER_REG, 0x10000);
    uint32_t end_lapic_timer = lapic_read(LAPIC_CURRENT_COUNT_REG);
    uint64_t end_tsc_timer = rdtsc();

    // Calibrate the LAPIC and TSC
    lapic_ticks_per_ms = (start_lapic_timer - end_lapic_timer) / 10;
    tsc_freq_hz = (end_tsc_timer - start_tsc_timer) * 100;
    tsc_boot_time = end_tsc_timer;

    log_line(LOG_SUCCESS, "CPU Frequency (TSC): %llu MHz", tsc_freq_hz / 1000000);

    // Configure the timer interrupt as periodic and set the idt entry to call
    lapic_write(LAPIC_LVT_TIMER_REG, LAPIC_LVT_VECTOR | LAPIC_LVT_PERIODIC);
    lapic_write(LAPIC_DIVIDE_REG, 0x3);

    // Set the timer to start and fire TIMER_FREQUENCY_HZ interrupts each second
    lapic_write(LAPIC_INITIAL_COUNT_REG, lapic_ticks_per_ms * (1000 / TIMER_FREQUENCY_HZ));
}

/**
 * @brief Return the current uptime in ticks
 * 
 * @return uint64_t The ticks
 */
uint64_t timer_get_uptime_ticks()
{
    return system_ticks;
}

/**
 * @brief Return the current uptime in milliseconds
 * 
 * @return uint64_t The milliseconds
 */
uint64_t timer_get_uptime_ms()
{
    uint64_t diff = rdtsc() - tsc_boot_time;
    return diff / (tsc_freq_hz / 1000);
}


extern struct task *task_current;
/**
 * @brief The ISR for the scheduler
 */
 struct cpu_status* timer_handler(struct cpu_status* context)
{
    system_ticks++;
    lapic_send_EOI();

    if(task_current)
    {
        // The time for this task has ended time for scheduling another
        task_current->ticks_remaining--;
        if(task_current->ticks_remaining == 0)
        {
            task_current->ticks_remaining = TASK_INITIAL_TICKS; // We restore them
            return scheduler_schedule(context);
        }
    }

    return context;
}

/**
 * @brief Blocks the execution for ms milliseconds
 * @param ms how many milliseconds to block
 */
 void timer_sleep(uint64_t ms)
 {
     uint64_t start = timer_get_uptime_ms();
     
     // Busy wait
     while (timer_get_uptime_ms() < start + ms) {
        asm volatile("pause");
     }
 }