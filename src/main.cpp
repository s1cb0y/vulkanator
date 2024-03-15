#include "vknator_engine.h"

int main (int argc, char* argv[]){

    vknator::VknatorEngine engine = vknator::VknatorEngine();
    if (engine.Init()){
        engine.Run();
        engine.Deinit();
    }
    return 0;
}