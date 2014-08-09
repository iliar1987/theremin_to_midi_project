#pragma once

#include "midi_sender.h"

#include <cmath>
#include <iostream>
#include <memory>

#include "boost\preprocessor\repetition\repeat.hpp"
#include "boost\preprocessor\seq\for_each_i.hpp"
#include "boost\preprocessor\tuple\elem.hpp"

#include "rapidjson\document.h"

#define DEF_VAR(r,suffix,i,elem) BOOST_PP_TUPLE_ELEM(2,0,elem) BOOST_PP_TUPLE_ELEM(2,1,elem) suffix

#include <exception>

namespace pltcc
{

	template <typename T> inline T db_to_lin(T value)
	{
		return std::pow(10.0, value/10.0);
	}
	template <typename T> inline T lin_to_db(T value)
	{
		return 10.0*std::log10(value);
	}

	template <typename T> class OptionallyLogarithmic
	{
		T value;
	public:
		operator T() { return GetLinear(); }
		OptionallyLogarithmic(T _value) { value = _value; }
		OptionallyLogarithmic() {}
		T GetLinear() { return value; }
		T GetDB() { return 10.0*std::log10(value); }
		void SetLinear(T _value) { value = _value; }
		void SetDB(T _value) { value = db_to_lin(_value); }
		void FromRapidJsonObject(rapidjson::Value &obj);
		OptionallyLogarithmic(rapidjson::Value &obj) { FromRapidJsonObject(obj); }
	};

#define NORMALIZER_FIELDS \
	((OptionallyLogarithmic<T>, minval))\
	((OptionallyLogarithmic<T>, maxval))\
	((bool,logarithmic))\
	((bool,cleave_top))\
	((bool,cleave_bottom))

	template<typename T> class Normalizer
	{
	protected:
		T normalize_any(T v, T a, T b)
		{
			return (v - a) / (b - a);
		}
	public:
		BOOST_PP_SEQ_FOR_EACH_I(DEF_VAR, ;,NORMALIZER_FIELDS)
		/*Normalizer(T minval, T maxval, bool logarithmic, bool cleave_top = true, bool cleave_bottom = true)
			: minval(minval), maxval(maxval), logarithmic(logarithmic), cleave_top(cleave_top), cleave_bottom(cleave_bottom)
		{}*/
		virtual T Normalize(T value);
		virtual void FromRapidJsonObject( rapidjson::Value &obj);
		virtual ~Normalizer() {}
	};

	template<typename T> std::ostream& operator << (std::ostream& out, Normalizer<T> no);

#define ValueToControllerConverter_FIELDS \
	((C, controller_min))((C,controller_max))

	template<typename T, typename C> class ValueToControllerConverter : public Normalizer<T>
	{
	public:
		BOOST_PP_SEQ_FOR_EACH_I(DEF_VAR, ;,ValueToControllerConverter_FIELDS)
		virtual C Convert(T value) = 0;
		/*ValueToControllerConverter(C controller_min, C controller_max, T minval, T maxval, bool logarithmic, bool cleave_top = true, bool cleave_bottom = true)
			:Normalizer(minval, maxval, logarithmic, cleave_top, cleave_bottom)
			, controller_min(controller_min), controller_max(controller_max)
		{}*/
		/*ValueToControllerConverter(const ValueToControllerConverterConfig<T,C> &config)
			: controller_min(config.controller_min),
			controller_max(config.controller_max),
			controller*/
		virtual void FromRapidJsonObject(rapidjson::Value &obj);
	};
	template<typename T, typename C> class LinearValueToControllerConverter : public ValueToControllerConverter<T,C>
	{
	public:
		C Convert(T value)
		{
			T n = Normalize(value);
			return static_cast<C>(std::round(n * (controller_max - controller_min) + controller_min));
		}
		/*LinearValueToControllerConverter(C controller_min, C controller_max, T minval, T maxval, bool logarithmic, bool cleave_top = true, bool cleave_bottom = true)
			:ValueToControllerConverter(controller_min, controller_max, minval, maxval, logarithmic, cleave_top, cleave_bottom)
		{}*/
	};

	typedef ValueToControllerConverter<float,BYTE> StandardTypeConverter;
	typedef LinearValueToControllerConverter<float, BYTE> StandardTypeLinearConverter;

#define PitchLevelToMidi_FIELDS \
	((OptionallyLogarithmic<float>,level_gate))\
	((int,channel_number))\
	((int, pitch_coupled_controller_number))\
	((int, level_coupled_controller_number))\
	((bool,send_pitch))\
	((bool,send_level))

	class PitchLevelToMidi
	{
		int my_midi_device_number;
	public:
		BOOST_PP_SEQ_FOR_EACH_I(DEF_VAR, ;, PitchLevelToMidi_FIELDS)
		/*int channel_number,
			pitch_coupled_controller_number,
			level_coupled_controller_number;
		float level_gate;*/
		std::shared_ptr<StandardTypeConverter> pitch_converter;
		std::shared_ptr<StandardTypeConverter> level_converter;
		//bool send_pitch, send_level;
		midis::MidiOutStream mos;
		void Start(int midi_device_number);
		int GetMidiDeviceNumber() { return my_midi_device_number; }
		void Stop();
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
		
		virtual ~PitchLevelToMidi()
		{
			Stop();
		}
		virtual void ConvertAndSend(float pitch, float level);
		virtual void FromRapidJsonObject( rapidjson::Value &obj);
		virtual void FromJsonFile(char* filename);
	};

	void InitializeDefaultPitchLevelToMidi(PitchLevelToMidi& p);
}

#if !PLTCC_H_DONT_CLEANUP
#undef DEF_VAR
#undef NORMALIZER_FIELDS
#undef ValueToControllerConverter_FIELDS
#undef PitchLevelToMidi_FIELDS
#endif
