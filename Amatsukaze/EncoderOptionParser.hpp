#pragma once

#include <string>
#include <regex>

#include "StringUtils.hpp"
#include "TranscodeSetting.hpp"
#include "avisynth.h"

enum ENUM_ENCODER_DEINT {
	ENCODER_DEINT_NONE,
	ENCODER_DEINT_30P,
	ENCODER_DEINT_24P,
	ENCODER_DEINT_60P,
	ENCODER_DEINT_VFR
};

struct EncoderOptionInfo {
	ENUM_ENCODER_DEINT deint;
	bool afsTimecode;
};

static std::vector<std::wstring> SplitOptions(const std::string& str)
{
	auto vec = string_internal::to_wstring(str);
	std::wstring wstr(vec.begin(), vec.end() - 1); // -1: null終端を取り除く
	std::wregex re(L"(([^\" ]+)|\"([^\"]+)\") *");
	std::wsregex_iterator it(wstr.begin(), wstr.end(), re);
	std::wsregex_iterator end;
	std::vector<std::wstring> argv;
	for (; it != end; ++it) {
		if ((*it)[2].matched) {
			argv.push_back((*it)[2].str());
		}
		else if ((*it)[3].matched) {
			argv.push_back((*it)[3].str());
		}
	}
	return argv;
}

EncoderOptionInfo ParseEncoderOption(ENUM_ENCODER encoder, const std::string& str)
{
	EncoderOptionInfo info = EncoderOptionInfo();

	if (encoder != ENCODER_QSVENC && encoder != ENCODER_NVENC) {
		return info;
	}

	auto argv = SplitOptions(str);
	int argc = (int)argv.size();

	for (int i = 0; i < argc; ++i) {
		auto& arg = argv[i];
		auto& next = (i + 1 < argc) ? argv[i + 1] : L"";
		if (arg == L"--vpp-deinterlace") {
			if (next == L"normal" || next == L"adaptive") {
				info.deint = ENCODER_DEINT_30P;
			}
			else if (next == L"it") {
				info.deint = ENCODER_DEINT_24P;
			}
			else if (next == L"bob") {
				info.deint = ENCODER_DEINT_60P;
			}
		}
		else if (arg == L"--vpp-afs") {
			bool is24 = false;
			bool timecode = false;
			bool drop = false;
			std::wregex re(L"([^=]+)=([^,]+),?");
			std::wsregex_iterator it(next.begin(), next.end(), re);
			std::wsregex_iterator end;
			std::vector<std::wstring> argv;
			for (; it != end; ++it) {
				auto key = (*it)[1].str();
				auto val = (*it)[2].str();
				std::transform(val.begin(), val.end(), val.begin(), ::tolower);
				if (key == L"24fps") {
					is24 = (val == L"1" || val == L"true");
				}
				else if (key == L"drop") {
					drop = (val == L"1" || val == L"true");
				}
				else if (key == L"timecode") {
					timecode = (val == L"1" || val == L"true");
				}
				else if (key == L"preset") {
					is24 = (val == L"24fps");
					drop = (val == L"double" || val == L"anime" ||
						val == L"cinema" || val == L"min_afterimg" || val == L"24fps");
				}
			}
			if (is24 && !drop) {
				THROW(ArgumentException, 
					"vpp-afsオプションに誤りがあります。24fps化する場合は間引き(drop)もonにする必要があります");
			}
			if (drop && !timecode) {
				THROW(ArgumentException,
					"vpp-afsで間引き(drop)がonの場合はタイムコード(timecode=true)が必須です");
			}
			if (timecode) {
				info.deint = ENCODER_DEINT_VFR;
				info.afsTimecode = true;
			}
			else {
				info.deint = is24 ? ENCODER_DEINT_24P : ENCODER_DEINT_30P;
				info.afsTimecode = false;
			}
		}
	}

	return info;
}

void PrintEncoderInfo(AMTContext& ctx, EncoderOptionInfo info) {
	switch (info.deint) {
	case ENCODER_DEINT_NONE:
		ctx.info("エンコーダでのインタレ解除: なし");
		break;
	case ENCODER_DEINT_24P:
		ctx.info("エンコーダでのインタレ解除: 24fps化");
		break;
	case ENCODER_DEINT_30P:
		ctx.info("エンコーダでのインタレ解除: 30fps化");
		break;
	case ENCODER_DEINT_60P:
		ctx.info("エンコーダでのインタレ解除: 60fps化");
		break;
	case ENCODER_DEINT_VFR:
		ctx.info("エンコーダでのインタレ解除: VFR化");
		break;
	}
}
