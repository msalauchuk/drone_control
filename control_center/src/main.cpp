#include "ControlCenter.h"
#include "config.h"
int main(int argc, char *argv[]) {
    // defining the cc
    ControlCenter cc;
    cc.init();
    print_parameters(); //print parameters to stdoutput for readability
    for (int t = 0; t< HORIZON; t ++ ){
        printf("______________________\nt =  %d\n\n", t);
        cc.await_sync(); // sync bloccante con redis
        //cc.print_status(); 
        cc.tick(); // performs the action in a given istant of time t based on current status
        cc.log(t); // logs data + monitors
    }
    if(DEBUG) {
        printf("finished simulation\nclosing conn...\n");
    }
    //cc.shutdown(); // cleans up everything
    return 0;
}