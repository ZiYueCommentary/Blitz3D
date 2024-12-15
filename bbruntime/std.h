#ifndef STD_H
#define STD_H

#define NOMINMAX

#include <Windows.h>

#include "constants.h"
#include "../config/config.h"
#include "../stdutil/stdutil.h"

#include <set>
#include <map>
#include <list>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>

#include <cmath>

#define ErrorLog(function, log) if (debug) { RTEX(log) } else { errorfunc = function; errorlog = log; }

#endif