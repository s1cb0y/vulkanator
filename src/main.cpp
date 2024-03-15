#include "vknator_engine.h"
#include <iostream>
#include "vknator_log.h"

int main (int argc, char* argv[]){
    vknator::Log::Init();
    vknator::VknatorEngine engine = vknator::VknatorEngine();
    if (engine.Init()){
        engine.Run();
        engine.Deinit();
    }
    return 0;
}