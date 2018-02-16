/*
Copyright 2015 - 2017 Andreas Chaitidis Andreas.Chaitidis@gmail.com
This program is free software : you can redistribute it and / or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with this program.If not, see <http://www.gnu.org/licenses/>.
*/

#include "LiPoCheck.h"
#include "config.h"

float batDc[2][11];

void initBattArray() {

  for (char i=0; i<2; i++) {
    for (char j=0; j<11; j++) {
      if (BATT_TYPE == 1) {
        batDc[i][j] = liionDc[i][j];
      }
      else {
        batDc[i][j] = lipoDc[i][j];
      }
    }
  }
  
}

int CapCheckPerc(float voltage, int cells) {
	float voltageCell = 0;
	int ind = 0;



	if (cells > 0)
	{
		voltageCell = (voltage / cells);
	}

	if (voltageCell >=4.20)
	{
		return (100);
	}

	while (!(voltageCell<=batDc[0][ind+1] && voltageCell > batDc[0][ind])&& ind<=10)
	{
		ind++;
	}
	

	if (voltageCell <= batDc[0][ind + 1] && voltageCell > batDc[0][ind])
	{
		float CapacPers = (((batDc[1][ind + 1] - batDc[1][ind])/ (batDc[0][ind + 1] - batDc[0][ind]))*(voltageCell - batDc[0][ind])) + batDc[1][ind];
	//	int intCapacPers = (int)(CapacPers * 100);
		return (CapacPers * 100);
	}
	else
	{
		return 0;
		
	}
}