#pragma once

#include "midi_sender.h"

#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <map>

#include <exception>

#include "json_struct_initializer.h"

namespace pltcc
{
	enum converter_input_type_enum
	{
		converter_type_pitch,
		converter_type_level,
		converter_type_invalid
	};

	using namespace rj;

	class RapidJsonInitializable
	{
	public:
		virtual void FromRapidJsonObject(rj::Value &obj) = 0;
	};

	template<typename C> class LimitedType
	{
	protected:
		C value;
		C min, max;
		friend void GetJsonValue(LimitedType<C> &param,rj::Value &obj);
	public:
		const C &absolute_min;
		const C &absolute_max;
		/*LimitedType(C _absolute_min, C _absolute_max)
			: absolute_min(min), absolute_max(max)
		{
			min = _absolute_min; max = _absolute_max;
		}*/
		void SetAbsoluteLimits(C _absolute_min, C _absolute_max)
		{
			min = _absolute_min; max = _absolute_max;
		}
		LimitedType() 
			: absolute_min(min), absolute_max(max)
		{}
		
		LimitedType<C>& operator = (double r) { value = static_cast<C>(std::round((max - min)*r + min)); return *this; }
		LimitedType<C>& operator = (std::string s);
		LimitedType<C>& operator = (C newval) { value = newval; return *this; }
		operator C () const { return value; }
	};

#define NORMALIZER_FIELDS \
	((OptionallyLogarithmic<T>, minval))\
	((OptionallyLogarithmic<T>, maxval))\
	((bool,logarithmic))\
	((bool,cleave_top))\
	((bool,cleave_bottom))

	template<typename T> class Normalizer
		: public virtual RapidJsonInitializable
	{
	protected:
		T normalize_any(T v, T a, T b)
		{
			return (v - a) / (b - a);
		}
	public:
		DEF_ALL_VARS(NORMALIZER_FIELDS)
		virtual T Normalize(T value);
		virtual void FromRapidJsonObject( rapidjson::Value &obj);
		virtual ~Normalizer() {}
	};

	template<typename T> std::ostream& operator << (std::ostream& out, const Normalizer<T> &no);

	template<typename C> static std::ostream& operator << (std::ostream &out, const LimitedType<C> &LT)
	{
		return out << "absolute max = " << (int)LT.absolute_max
			<< std::endl
			<< "absolute min = " << (int)LT.absolute_min
			<< std::endl
			<< "my value = " << (int)(C)LT
			<< std::endl;
	}

#define ValueConverter_FIELDS \
	((LimitedType<C>,controller_min))\
	((LimitedType<C>,controller_max))

	template<typename T, typename C> class ValueConverter 
		: public virtual Normalizer<T>
		, public virtual RapidJsonInitializable
	{
	public:
		/*ValueConverter(C absolute_min, C absolute_max)
			: controller_min(absolute_min, absolute_max)
			, controller_max(absolute_min, absolute_max)
		{}*/
		ValueConverter() {}

		void SetAbsoluteLimits(C absolute_min, C absolute_max)
		{
			//std::cerr << "in valueconverter::setabsolutelimits " << std::endl;
			//std::cerr << (int)absolute_min << " " << (int)absolute_max << std::endl;
			controller_min.SetAbsoluteLimits(absolute_min, absolute_max);
			controller_max.SetAbsoluteLimits(absolute_min, absolute_max);
		}
		DEF_ALL_VARS(ValueConverter_FIELDS)
			virtual C Convert(T value) = 0;
		virtual void FromRapidJsonObject(rapidjson::Value &obj);
		virtual ~ValueConverter() {}
	};

	template<typename T, typename C> static std::ostream& operator << (std::ostream& out, const ValueConverter<T, C> &VC)
	{
		return out << (const Normalizer<T>&)VC
			<< "minimum controller value: " << VC.controller_min
			<< std::endl
			<< "maximum controller value: " << VC.controller_max
			<< std::endl;
	}

	template<typename T, typename C> class LinearValueConverter : public virtual ValueConverter<T, C>
	{
	public:
		C Convert(T value)
		{
			T n = Normalize(value);
			T x = std::round(n * (controller_max - controller_min) + controller_min);
			C c;
			if (x < controller_min.absolute_min)
				c = controller_min.absolute_min;
			else if (x > controller_max.absolute_max)
				c = controller_max.absolute_max;
			else
				c = static_cast<C>(x);
			return c;
		}
		virtual ~LinearValueConverter() {}
	};

#define ConvertAndSender_FIELDS \
	((int, channel_number))((bool,send))
		template<typename T> class ConvertAndSender
			: public virtual RapidJsonInitializable
		{
		protected:
			int last_sent_value;
		public:
			DEF_ALL_VARS(ConvertAndSender_FIELDS)
			virtual void ConvertAndSend(midis::MidiOutStream &midi_o_s,T value)=0;
			virtual void FromRapidJsonObject(rj::Value &obj);
			virtual int LastSentValue() { return last_sent_value; }
			virtual std::string GetOutputType()=0;
			virtual ~ConvertAndSender() {}
		};

#define ValueToControllerConverter_FIELDS ((int, controller_number))

	class ValueToControllerConverter 
		: public virtual ValueConverter<float, BYTE>
		, public virtual ConvertAndSender<float>
		, public virtual RapidJsonInitializable
	{
	public:
		DEF_ALL_VARS(ValueToControllerConverter_FIELDS)
		ValueToControllerConverter()
		{
			ValueConverter::SetAbsoluteLimits(midis::CONTROLLER_MIN, midis::CONTROLLER_MAX);
		}
		virtual void FromRapidJsonObject(rj::Value &obj);
		void ConvertAndSend(midis::MidiOutStream &midi_o_s, float value);
		std::string GetOutputType();
		virtual ~ValueToControllerConverter() {}
	};

	class LinearValueToControllerConverter
		: public virtual LinearValueConverter<float, BYTE>
		, public virtual ValueToControllerConverter
	{};

	class ValueToPitchBendConverter 
		: public virtual ValueConverter<float,short>
		, public virtual ConvertAndSender<float>
		, public virtual RapidJsonInitializable
	{
	public:
		ValueToPitchBendConverter()
		{
			ValueConverter::SetAbsoluteLimits(midis::PITCH_BEND_MIN, midis::PITCH_BEND_MAX);
		}
		std::string GetOutputType()
		{
			return std::string("PB");
		}
		virtual void FromRapidJsonObject(rj::Value &obj);
		void ConvertAndSend(midis::MidiOutStream &midi_o_s, float value);
		virtual ~ValueToPitchBendConverter() {}
	};

	class LinearValueToPitchBendConverter
		: public virtual LinearValueConverter<float, short>
		, public virtual ValueToPitchBendConverter
	{};
	
	/*typedef ValueToControllerConverter<float,BYTE> StandardTypeConverter;
	typedef LinearValueToControllerConverter<float, BYTE> StandardTypeLinearConverter;
*/
	struct ConverterWithType
	{
		ConvertAndSender<float>* converter;
		converter_input_type_enum converter_input_type;
		ConverterWithType() 
		{ converter = 0;
		converter_input_type = converter_type_invalid; }
	};
//#define PitchLevelToMidi_FIELDS \
//	((std::string,pitch_converter_type))\
//	((std::string,level_converter_type))
//	//((OptionallyLogarithmic<float>, level_gate))

	class PitchLevelToMidi
		: public virtual RapidJsonInitializable
	{
		typedef std::map<int, ConverterWithType> ConverterContainer;
		ConverterContainer converters;
	public:
		~PitchLevelToMidi()
		{
			for (ConverterContainer::iterator it
				= converters.begin()
				; it != converters.end()
				; it++)
					if (it->second.converter)
						delete (it->second.converter);
		}
		std::ostream& OutputLastSentValues(std::ostream &out);
		void ToggleByKey(char key);
	//	DEF_ALL_VARS(PitchLevelToMidi_FIELDS)
		virtual void ConvertPitchLevelAndSend(midis::MidiOutStream &mos, float pitch, float level);
		virtual void FromRapidJsonObject( rapidjson::Value &obj);
		virtual void FromJsonFile(char* filename);
	};

}

#include "fancy_json_macros.h"
