#include "Drone.h"

int main() {

    Swarm swarm = Swarm();
    swarm.init();
    swarm.init_drones(DRONES_COUNT);
    int t = 0;
    
    while(t<HORIZON) {
        printf("_____________________________\nt = %d\n\n",t);
        swarm.await_sync(); 
        swarm.tick(t);
        swarm.log();
        t++;
    }

    return 0;
}
