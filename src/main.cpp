#include "global.hpp"
#include "joybus.hpp"

#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/regs/rosc.h"

#include "pico/time.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "pico/util/queue.h"

#include <array>
#include <functional>
#include "string.h"

#include <icdf.hpp>

uint32_t rnd(void){
    int k, random=0;
    volatile uint32_t *rnd_reg=(uint32_t *)(ROSC_BASE + ROSC_RANDOMBIT_OFFSET);
    
    for(k=0;k<32;k++){
    
    random = random << 1;
    random=random + (0x00000001 & (*rnd_reg));

    }
    return random;
}

#define EMULATE_PLAYER_BEHAVIOUR 0

#define DASHES 0
#define JUMP_ATTACK 1

#if EMULATE_PLAYER_BEHAVIOUR
/* The test will iterate over the 1/sweepingPeriod splits of the distribution and pick a random position in that window (variance reduction)
This means that to get a statistically correct estimate, you must ensure you trim (naively, ex. always the end) your results to be a contiguous range
whose size is a multiple of the sweeping period. For ex, 1700 measures & sweeping period of 200 => use the 1600 first.
Otherwise, you'd have 900 values in one half of the distribution and 800 in the other, which will obviously yield bullshit results. */
const int sweepingPeriod = 100;

// Normal law sigma => player success rate table
// 1.044 : 95%
// 2.089 : 90%
// 3.133 : 85%
// 3.48  : 83.34%
// 3.483 : 83.33%
// 4.178 : 80%
// 5.225 : 75%
// 6.287 : 70%
const double playerNormalLawSigma = 2.089;
// Note for <= 80% test you run the risk of reaching -2 or +2 frames offsets (you should increase the icdf cramping to compensate)

int iSweep = sweepingPeriod/2;
double get_player_imperfection_offset_and_advance_sweep() {
    double rand = ((double)rnd()) / 0xffffffff;
    double p = (((double)iSweep)+rand) / sweepingPeriod;
    double icdf_result = inverse_of_normal_cdf(p, 0, 1);
    if (++iSweep == sweepingPeriod) iSweep = 0;
    return icdf_result * playerNormalLawSigma;
}
#endif

void await_time32us(uint32_t target) {
    while ( (time_us_32() - target) & (1 << 31) );
}

enum class State {
    LEFT,
    RIGHT,
    NONE,
    UP,
    A,
    Y
};

struct Bridge {
    volatile State state = State::NONE;
}; 
Bridge bridge;

void local_main() {

    #if DASHES
    bool dirRight = true;
    #endif

    multicore_fifo_push_blocking((uintptr_t)&bridge);

    int a = 0;
    int b = 1;

    gpio_init(a);
    gpio_set_dir(a, GPIO_OUT);
    gpio_put(a, 1);

    gpio_init(b);
    gpio_set_dir(b, GPIO_OUT);
    gpio_put(b, 1);

    uint32_t timestamp = time_us_32();
    uint32_t awaitTransferTimestamp = 0;

    sleep_ms(2000);
    bridge.state = State::A;
    sleep_ms(200);
    bridge.state = State::NONE;
    sleep_ms(200);
    bridge.state = State::A;
    sleep_ms(200);
    bridge.state = State::UP;
    sleep_ms(300);
    bridge.state = State::NONE;
    sleep_ms(200);
    bridge.state = State::A;
    sleep_ms(200);
    bridge.state = State::NONE;
    sleep_ms(200);
    bridge.state = State::A;
    sleep_ms(100);
    bridge.state = State::NONE;
    sleep_ms(2000);

    while (true) {

        timestamp = time_us_32();

        while (time_us_32() - timestamp <= 1'000);
        bridge.state = State::NONE;

        while (time_us_32() - timestamp <= 40'000);

        #if JUMP_ATTACK
        bridge.state = State::Y;
        gpio_set_dir(a, GPIO_OUT);
        gpio_put(a, 0);
        #endif
        #if DASHES
        bridge.state = dirRight ? State::RIGHT : State::LEFT;
        dirRight = !dirRight;
        #endif

        while (time_us_32() - timestamp <= 80'000);
        bridge.state = State::NONE;
        gpio_put(a, 1);

        int playerImperfectionOffset = 0;
        #if EMULATE_PLAYER_BEHAVIOUR
        playerImperfectionOffset = get_player_imperfection_offset_and_advance_sweep() * 1'000.;
        #endif

        int threshold = 40'000 + (int)(34*50'000/3.) + playerImperfectionOffset;
        while (time_us_32() - timestamp <= threshold);

        #if JUMP_ATTACK
        bridge.state = State::A;
        gpio_set_dir(b, GPIO_OUT);
        gpio_put(b, 0);
        #endif

        while (time_us_32() - timestamp <= threshold + 40'000 + playerImperfectionOffset);
        bridge.state = State::NONE;
        gpio_put(b, 1);

        while (time_us_32() - timestamp <= threshold + 500'000 + playerImperfectionOffset); // 30f (move startup + hitlag + landing lag)

        sleep_us(rnd() % 16'667);
        bridge.state = State::NONE;
    }   
}

void core1_entry() {
    set_sys_clock_khz(us*1000, true);

    Bridge* localBridge = (Bridge*) multicore_fifo_pop_blocking();

    std::function<GCReport()> callback = [localBridge](){ // Quand on passe des choses à un lambda on ne peut plus le convertir en pointeur vers fonction //TODO
        GCReport gcReport = defaultGcReport;
        State state = localBridge->state;

        switch (state) {
        case State::NONE:
            break;
        case State::UP:
            gcReport.yStick = 208;
            break;
         case State::A:
            gcReport.a = 1;
            break;
         case State::Y:
            gcReport.y = 1;
            break;
         case State::LEFT:
            gcReport.xStick = 69; // 72=-0.7; 70=-0.725=turnaround ; 68=-0.75=dash // 69=turnaround
            break;
         case State::RIGHT:
            gcReport.xStick = 187; // 184=0.7 ; 186=0.725=turnaround; 188=0.75=dash //187=turnaround
            break;
        }

        return gcReport;
    };

    CommunicationProtocols::Joybus::enterMode(22, callback); // used to be 17
}

int main() {
    // Clock at 125MHz
    set_sys_clock_khz(us*1000, true);
    
    initialize_uart();

    gpio_init(25);
    gpio_set_dir(25, GPIO_OUT);

    multicore_launch_core1(core1_entry);

    local_main();
}