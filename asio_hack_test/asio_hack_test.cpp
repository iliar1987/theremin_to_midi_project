#define AH_DEBUG

#include <Windows.h>
#include "asio.h"

#include "asio_hacks.h"


#include <iostream>

using namespace ah;

int main()
{
	int n = 128;
	int *buff = new int[n];
	int i;
	for ( i=0 ; i < n ; i++ )
		buff[i] = 1;

	GenericBuffer& vtb = VariableTypeBuffer<double>((void*)buff);
	GenericBuffer &vtb2 = VariableTypeBuffer<short>((void*)buff);
	GenericBuffer &vtb3 = VariableTypeBuffer<INT24>((void*)((char*)buff + 2));

	*vtb2.GetElement(1) = 5;
	((INT8*)buff)[3]=3;
	std::cout<<sizeof(short)<<"\t" << sizeof(INT24) << std::endl;
	std::cout << (int)*vtb.GetElement(0) << "\t" << std::hex<<buff[0]<< "\t24bit: "<<std::hex<<(int)*vtb3.GetElement(0)<<"\t"<<std::hex<<(int)*vtb3.GetElement(1) <<std::endl;
	std::cout << (double)*vtb.GetElement(1) <<std::endl;
	std::cout << vtb.GetNormalizationConstant() << "\t" << vtb2.GetNormalizationConstant() << std::endl;


	delete[](buff);

	return 0;
}