#include "System.h"
#include <iostream>

using namespace RAIN_IMU;
using namespace std;

int main(void)
{
	System imusystem;
	
	cout << "main running " << endl;

	imusystem.RunEKF();
}