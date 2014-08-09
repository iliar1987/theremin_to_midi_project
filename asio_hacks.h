#pragma once

#include <memory>

#ifdef AH_DEBUG
#include <cassert>
#endif

#ifdef AH_DEBUG
#include <iostream>
#endif

#include <exception>

struct INT24
{
	INT8 bytes[3];
	operator int ()
	{
		return bytes[0] + (bytes[1]<<8) + (bytes[2]<<16);
	}
	INT24(int i) {bytes[0] = i&0xFF ; bytes[1] = i&0xFF00 ; bytes[2] = i&0xFF0000;}
};

namespace ah{

class GenericElementAccessor
{
protected:
	void *base_sample;
public:
	virtual ~GenericElementAccessor() {}
	GenericElementAccessor(void *sample)
	{
		base_sample=sample;
	}
	virtual int SampleSize() = 0;
	virtual void operator = (short d) = 0;
	virtual void operator = (double d) = 0;
	virtual void operator = (float d) = 0;
	virtual void operator = (long l) = 0;
	virtual void operator = (int l) = 0;
	/*virtual operator double() {return static_cast<double> (this->operator float())}
	virtual operator long() {return static_cast<long> (this->operator int())}*/
	virtual operator short() = 0;
	virtual operator int() = 0;
	virtual operator float() = 0;
	virtual operator long() = 0;
	virtual operator double() = 0;
};

template<typename sample_t> class VariableTypeElementAccessor : public GenericElementAccessor
{
private:
	sample_t* &mysample;
public:
	int SampleSize() {return sizeof(sample_t);}
	sample_t *GetSampleP() {return mysample;}
	sample_t& GetSample() {return *mysample;}
	VariableTypeElementAccessor(sample_t *sample) 
		: GenericElementAccessor(reinterpret_cast<void*>(sample))
		,mysample(reinterpret_cast<sample_t* &>(GenericElementAccessor::base_sample))
	{ }
	void operator = (double d) {*mysample = static_cast<sample_t>(d);}
	void operator = (short d) {*mysample = static_cast<sample_t>(d);}
	void operator = (float d) {*mysample = static_cast<sample_t>(d);}
	void operator = (long l) {*mysample = static_cast<sample_t>(l);}
	void operator = (int l) {*mysample = static_cast<sample_t>(l);}
	operator short() {return static_cast<short>(*mysample);}
	operator int() {return static_cast<int>(*mysample);}
	operator long() {return static_cast<long>(*mysample);}
	operator double() {return static_cast<double>(*mysample);}
	operator float() {return static_cast<float>(*mysample);}
	//typedef sample_t mysample_t;
};

class GenericBuffer
{
public:
	virtual double GetNormalizationConstant() = 0;
	virtual ~GenericBuffer() {}
	virtual int SampleSize() = 0;
	virtual std::shared_ptr<GenericElementAccessor> GetElement(unsigned int i) = 0;
};

template<typename sample_t> class VariableTypeBuffer : public GenericBuffer
{
private:
	sample_t *buff;
public:
#ifdef AH_DEBUG
	//~VariableTypeBuffer() {std::cout<<"In destructor"<<std::endl;}
#endif
	double GetNormalizationConstant();
	virtual int SampleSize() {return sizeof(sample_t);}
	VariableTypeBuffer(void *orig_buff)
	{
		buff = reinterpret_cast<sample_t*>(orig_buff);
	}
	sample_t* GetBuffer() {return buff;}
	std::shared_ptr<GenericElementAccessor> GetElement (unsigned int i)
	{
		sample_t* inbuff_ptr = &(buff[i]);
		return std::shared_ptr<GenericElementAccessor>(
				(GenericElementAccessor*)
				(new VariableTypeElementAccessor<sample_t>( inbuff_ptr ))
			);
	}
};

std::unique_ptr<GenericBuffer> NewGenericBufferFromASIO_Sample_T(void* buff,::ASIOSampleType asio_ST_code)
{
	GenericBuffer* ret_p;
#ifdef AH_DEBUG
	//std::cout<<asio_ST_code;
#endif
	switch (asio_ST_code)
	{
	case ::ASIOSTFloat64LSB:
		ret_p = new VariableTypeBuffer<double>((double*)buff);
		break;
	case ::ASIOSTFloat32LSB:
		ret_p = new VariableTypeBuffer<float>((float*)buff);
		break;
	case ::ASIOSTInt16LSB:
		ret_p = new VariableTypeBuffer<INT16>((INT16*)buff);
		break;
	case ::ASIOSTInt32LSB:
		ret_p = new VariableTypeBuffer<INT32>((INT32*)buff);
		break;
	case ::ASIOSTInt24LSB:
		ret_p = new VariableTypeBuffer<INT24>((INT24*)buff);
		break;
	default:
#ifdef AH_DEBUG
		std::cerr<<"Unsupported data type, asio sample type code: "<<asio_ST_code<<std::endl;
#endif
		throw("Unsupported data type");
	}
	
	return std::unique_ptr<GenericBuffer>(ret_p);
}

inline double VariableTypeBuffer<INT16>::GetNormalizationConstant() {return 1.0/MAXINT16;}
inline double VariableTypeBuffer<INT32>::GetNormalizationConstant() {return 1.0/MAXINT32;}
inline double VariableTypeBuffer<INT24>::GetNormalizationConstant() {return 1.0/(1<<23);}
inline double VariableTypeBuffer<double>::GetNormalizationConstant() {return 1.0;}
inline double VariableTypeBuffer<float>::GetNormalizationConstant() {return 1.0;}

}

//#if USE_INT64
//#define _NANOSECONDS_S_CONVERSION(a) (static_cast<long long>(a.lo) + (static_cast<long long>(a.hi) << 32))
//struct NANOSECONDS_S
//{
////	template <typename ASIO_64bitTimeType> explicit NANOSECONDS_S(const ASIO_64bitTimeType &time)
////	{
////		//nanoseconds = static_cast<long long>(time.lo) + (static_cast<long long>(time.hi) << 32);
////		this->operator = (time);
////#ifdef AH_DEBUG
////		assert(sizeof(*this) == 8);
////#endif
////	}
////	/*explicit */NANOSECONDS_S(long long value) { this->operator= (value); }
////	template <class ASIO_64bitTimeType> NANOSECONDS_S& operator = (const ASIO_64bitTimeType &time)
////	{
////		nanoseconds = static_cast<long long>(time.lo) + (static_cast<long long>(time.hi) << 32);
////		return *this;
////	}
//	long long nanoseconds;
//	operator long long() const { return nanoseconds; }
//	/*NANOSECONDS_S& operator = (long long val)
//	{
//		nanoseconds = val;
//		return *this;
//	}*/
//	NANOSECONDS_S& operator = (long long value) { nanoseconds = value; return *this; }
//	NANOSECONDS_S& operator = (const NANOSECONDS_S &other) 
//		{ nanoseconds = other.nanoseconds; return *this; }
//	//Default constructor
//	NANOSECONDS_S() {
//		nanoseconds = 0;
//	}
//	operator long long &  () { return nanoseconds; }
//	/*explicit */NANOSECONDS_S(long long value) { this->operator= (value); }
//
//	explicit NANOSECONDS_S(const ASIOSamples &time)
//	{
//		//nanoseconds = static_cast<long long>(time.lo) + (static_cast<long long>(time.hi) << 32);
//		this->operator = (time);
//#ifdef AH_DEBUG
//		assert(sizeof(*this) == 8);
//#endif
//	}
//	explicit NANOSECONDS_S(const ASIOTimeStamp &time)
//	{
//		//nanoseconds = static_cast<long long>(time.lo) + (static_cast<long long>(time.hi) << 32);
//		this->operator = (time);
//#ifdef AH_DEBUG
//		assert(sizeof(*this) == 8);
//#endif
//	}
//	NANOSECONDS_S& operator = (const ASIOSamples &time)
//	{
//		nanoseconds = _NANOSECONDS_S_CONVERSION(time);
//		return *this;
//	}
//	NANOSECONDS_S& operator = (const ASIOTimeStamp &time)
//	{
//		nanoseconds = _NANOSECONDS_S_CONVERSION(time);
//		return *this;
//	}
//};
//typedef struct NANOSECONDS_S NANOSECONDS_T;

#ifndef USE_INT64
#define USE_INT64 1
#endif

#if USE_INT64
typedef long long NANOSECONDS_T;
#else
typedef double NANOSECONDS_T;
#endif

//----------------------------------------------------------------------------------
// conversion from 64 bit ASIOSample/ASIOTimeStamp to double float

#if USE_INT64
//#define ASIO64toNanoSeconds(a)  NANOSECONDS_S(a)
#define ASIO64toNanoSeconds(a)  ((long long)(a).lo + (((long long)(a).hi) << 32))
#else
const double twoRaisedTo32 = 4294967296.;
#define ASIO64toNanoSeconds(a)  ((a).lo + (a).hi * twoRaisedTo32)
#endif
