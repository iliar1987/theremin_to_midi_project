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
	using namespace rj;

	class RapidJsonInitializable
	{
	public:
		virtual void FromRapidJsonObject(rj::Value &obj) = 0;
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

	template<typename T> std::ostream& operator << (std::ostream& out, Normalizer<T> no);

#define ValueConverter_FIELDS \
	((C, controller_min))((C, controller_max))

	template<typename T, typename C> class ValueConverter 
		: public virtual Normalizer<T>
		, public virtual RapidJsonInitializable
	{
	public:
		DEF_ALL_VARS(ValueConverter_FIELDS)
			virtual C Convert(T value) = 0;
		virtual void FromRapidJsonObject(rapidjson::Value &obj);
		virtual ~ValueConverter() {}
	};
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
		virtual void FromRapidJsonObject(rj::Value &obj);
		void ConvertAndSend(midis::MidiOutStream &midi_o_s, float value);
		virtual ~ValueToControllerConverter() {}
	};

	class LinearValueToControllerConverter
		: public virtual ValueToControllerConverter
		, public virtual LinearValueConverter<float, BYTE>
	{};

	class ValueToPitchBendConverter 
		: public virtual ValueConverter<float,short>
		, public virtual ConvertAndSender<float>
		, public virtual RapidJsonInitializable
	{
	public:
		virtual void FromRapidJsonObject(rj::Value &obj);
		void ConvertAndSend(midis::MidiOutStream &midi_o_s, float value);
		virtual ~ValueToPitchBendConverter() {}
	};

	class LinearValueToPitchBendConverter
		: public virtual ValueToPitchBendConverter
		, public virtual LinearValueConverter<float, short>
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
	{
		int my_midi_device_number;
	public:
		DEF_ALL_VARS(PitchLevelToMidi_FIELDS)
		/*int channel_number,
			pitch_coupled_controller_number,
			level_coupled_controller_number;
		float level_gate;*/
		std::shared_ptr<ConvertAndSender<float>> pitch_converter;
		std::shared_ptr<ConvertAndSender<float>> level_converter;
		//bool send_pitch, send_level;
		midis::MidiOutStream mos;
		void Start(int midi_device_number);
		void Start(std::string midi_out_device_name);
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

//	void InitializeDefaultPitchLevelToMidi(PitchLevelToMidi& p);
}
