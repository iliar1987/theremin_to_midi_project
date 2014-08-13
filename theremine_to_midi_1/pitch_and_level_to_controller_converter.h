#pragma once

#include "midi_sender.h"

#include <cmath>
#include <iostream>
#include <memory>
#include <string>

#include <exception>

#include "json_struct_initializer.h"

namespace pltcc
{
	static const short PITCH_BEND_MIN = -0x2000;
	static const short PITCH_BEND_MAX = 0x1fff;
	static const BYTE CONTROLLER_MIN = 0;
	static const BYTE CONTROLLER_MAX = 127;

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
			return static_cast<C>(std::round(n * (controller_max - controller_min) + controller_min));
		}
		virtual ~LinearValueConverter() {}
	};

#define ConvertAndSender_FIELDS \
	((int,channel_number))
		template<typename T> class ConvertAndSender
			: public virtual RapidJsonInitializable
		{
		public:
			DEF_ALL_VARS(ConvertAndSender_FIELDS)
			virtual void ConvertAndSend(midis::MidiOutStream &midi_o_s,T value)=0;
			virtual void FromRapidJsonObject(rj::Value &obj);
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
			ValueConverter::SetAbsoluteLimits(CONTROLLER_MIN, CONTROLLER_MAX);
		}
		virtual void FromRapidJsonObject(rj::Value &obj);
		void ConvertAndSend(midis::MidiOutStream &midi_o_s, float value);
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
			ValueConverter::SetAbsoluteLimits(PITCH_BEND_MIN, PITCH_BEND_MAX);
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
#define PitchLevelToMidi_FIELDS \
	((bool,send_pitch))\
	((bool,send_level))\
	((std::string,pitch_converter_type))\
	((std::string,level_converter_type))
	//((OptionallyLogarithmic<float>, level_gate))

	class PitchLevelToMidi
		: public virtual RapidJsonInitializable
	{
		int my_midi_device_number;
		ConvertAndSender<float>* pitch_converter=0;
		ConvertAndSender<float>* level_converter=0;
	public:
		~PitchLevelToMidi()
		{
			if (pitch_converter) delete pitch_converter;
			if (level_converter) delete level_converter;
		}
		DEF_ALL_VARS(PitchLevelToMidi_FIELDS)
		/*int channel_number,
			pitch_coupled_controller_number,
			level_coupled_controller_number;
		float level_gate;*/
		//bool send_pitch, send_level;
		midis::MidiOutStream mos;
		/*void Start(int midi_device_number);
		void Start(std::string midi_out_device_name);*/
		int GetMidiDeviceNumber() { return my_midi_device_number; }
		//void Stop();
		/*PitchLevelToMidi(std::shared_ptr<StandardTypeConverter> pitch_converter,
		std::shared_ptr<StandardTypeConverter> level_converter,
		int channel_number,
		int pitch_coupled_controller_number,
		int level_coupled_controller_number)
		: pitch_converter(pitch_converter), level_converter(level_converter)
		, channel_number(channel_number)
		, pitch_coupled_controller_number(pitch_coupled_controller_number)
		, level_coupled_controller_number(level_coupled_controller_number)
		{}*/
		
		/*virtual ~PitchLevelToMidi()
		{
			Stop();
		}*/
		virtual void ConvertPitchLevelAndSend(midis::MidiOutStream &mos, float pitch, float level);
		virtual void FromRapidJsonObject( rapidjson::Value &obj);
		virtual void FromJsonFile(char* filename);
	};

//	void InitializeDefaultPitchLevelToMidi(PitchLevelToMidi& p);
}

#include "fancy_json_macros.h"
