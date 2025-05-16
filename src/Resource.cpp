#include "Resource.h"

#include<string>

Resource::Resource(std::string const& loc, bool dir) : kicaruib(loc), directory(dir){}

Resource::~Resource(){
    if (data != nullptr){
        delete[] data;
        data = nullptr;
    }
}