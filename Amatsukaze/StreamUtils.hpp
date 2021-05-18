/**
* Memory and Stream utility
* Copyright (c) 2017-2019 Nekopanda
*
* This software is released under the MIT License.
* http://opensource.org/licenses/mit-license.php
*/
#pragma once

#include <algorithm>
#include <vector>
#include <array>
#include <map>
#include <set>
#include <fstream>
#include <cctype>
#include <locale>
#include <codecvt>

#include "CoreUtils.hpp"
#include "OSUtil.hpp"
#include "StringUtils.hpp"

enum {
	TS_SYNC_BYTE = 0x47,

	TS_PACKET_LENGTH = 188,
	TS_PACKET_LENGTH2 = 192,

	MAX_PID = 0x1FFF,

	MPEG_CLOCK_HZ = 90000, // MPEG2,H264,H265��PTS��90kHz�P�ʂƂȂ��Ă���
};

inline static int nblocks(int n, int block)
{
	return (n + block - 1) / block;
}

/** @brief shift�����E�V�t�g����mask��bit�����Ԃ�(bit shift mask) */
template <typename T>
T bsm(T v, int shift, int mask) {
	return (v >> shift) & ((T(1) << mask) - 1);
}

/** @brief mask��bit����shift�������V�t�g���ď�������(bit mask shift) */
template <typename T, typename U>
void bms(T& v, U data, int shift, int mask) {
	v |= (data & ((T(1) << mask) - 1)) << shift;
}

template<int bytes, typename T>
T readN(const uint8_t* ptr) {
	T r = 0;
	for (int i = 0; i < bytes; ++i) {
		r = r | (T(ptr[i]) << ((bytes - i - 1) * 8));
	}
	return r;
}
uint16_t read16(const uint8_t* ptr) { return readN<2, uint16_t>(ptr); }
uint32_t read24(const uint8_t* ptr) { return readN<3, uint32_t>(ptr); }
uint32_t read32(const uint8_t* ptr) { return readN<4, uint32_t>(ptr); }
uint64_t read40(const uint8_t* ptr) { return readN<5, uint64_t>(ptr); }
uint64_t read48(const uint8_t* ptr) { return readN<6, uint64_t>(ptr); }

template<int bytes, typename T>
void writeN(uint8_t* ptr, T w) {
	for (int i = 0; i < bytes; ++i) {
		ptr[i] = uint8_t(w >> ((bytes - i - 1) * 8));
	}
}
void write16(uint8_t* ptr, uint16_t w) { writeN<2, uint16_t>(ptr, w); }
void write24(uint8_t* ptr, uint32_t w) { writeN<3, uint32_t>(ptr, w); }
void write32(uint8_t* ptr, uint32_t w) { writeN<4, uint32_t>(ptr, w); }
void write40(uint8_t* ptr, uint64_t w) { writeN<5, uint64_t>(ptr, w); }
void write48(uint8_t* ptr, uint64_t w) { writeN<6, uint64_t>(ptr, w); }


class BitReader {
public:
	BitReader(MemoryChunk data)
		: data(data)
		, offset(0)
		, filled(0)
	{
		fill();
	}

	bool canRead(int bits) {
		return (filled + ((int)data.length - offset) * 8) >= bits;
	}

	// bits�r�b�g�ǂ�Ői�߂�
	// bits <= 32
	template <int bits>
	uint32_t read() {
		return readn(bits);
	}

	uint32_t readn(int bits) {
		if (bits <= filled) {
			return read_(bits);
		}
		fill();
		if (bits > filled) {
			throw EOFException("BitReader.read�ŃI�[�o�[����");
		}
		return read_(bits);
	}

	// bits�r�b�g�ǂނ���
	// bits <= 32
	template <int bits>
	uint32_t next() {
		return nextn(bits);
	}

	uint32_t nextn(int bits) {
		if (bits <= filled) {
			return next_(bits);
		}
		fill();
		if (bits > filled) {
			throw EOFException("BitReader.next�ŃI�[�o�[����");
		}
		return next_(bits);
	}

	int32_t readExpGolomSigned() {
		static int table[] = { 1, -1 };
		uint32_t v = readExpGolom() + 1;
		return (v >> 1) * table[v & 1];
	}

	uint32_t readExpGolom() {
		uint64_t masked = bsm(current, 0, filled);
		if (masked == 0) {
			fill();
			masked = bsm(current, 0, filled);
			if (masked == 0) {
				throw EOFException("BitReader.readExpGolom�ŃI�[�o�[����");
			}
		}
		int bodyLen = filled - __builtin_clzl(masked);
		filled -= bodyLen - 1;
		if (bodyLen > filled) {
			fill();
			if (bodyLen > filled) {
				throw EOFException("BitReader.readExpGolom�ŃI�[�o�[����");
			}
		}
		int shift = filled - bodyLen;
		filled -= bodyLen;
		return (uint32_t)bsm(current, shift, bodyLen) - 1;
	}

	void skip(int bits) {
		if (filled > bits) {
			filled -= bits;
		}
		else {
			// ��fill����Ă��镪������
			bits -= filled;
			filled = 0;
			// ����Ńo�C�g�A���C�������̂Ŏc��o�C�g���X�L�b�v
			int skipBytes = bits / 8;
			offset += skipBytes;
			if (offset > (int)data.length) {
				throw EOFException("BitReader.skip�ŃI�[�o�[����");
			}
			bits -= skipBytes * 8;
			// ����1��fill���Ďc�����r�b�g��������
			fill();
			if (bits > filled) {
				throw EOFException("BitReader.skip�ŃI�[�o�[����");
			}
			filled -= bits;
		}
	}

	// ���̃o�C�g���E�܂ł̃r�b�g���̂Ă�
	void byteAlign() {
		fill();
		filled &= ~7;
	}

	// �ǂ񂾃o�C�g���i���r���[�ȕ����܂œǂ񂾏ꍇ���P�o�C�g�Ƃ��Čv�Z�j
	int numReadBytes() {
		return offset - (filled / 8);
	}

private:
	MemoryChunk data;
	int offset;
	uint64_t current;
	int filled;

	void fill() {
		while (filled + 8 <= 64 && offset < (int)data.length) readByte();
	}

	void readByte() {
		current = (current << 8) | data.data[offset++];
		filled += 8;
	}

	uint32_t read_(int bits) {
		int shift = filled - bits;
		filled -= bits;
		return (uint32_t)bsm(current, shift, bits);
	}

	uint32_t next_(int bits) {
		int shift = filled - bits;
		return (uint32_t)bsm(current, shift, bits);
	}
};

class BitWriter {
public:
	BitWriter(AutoBuffer& dst)
		: dst(dst)
		, current(0)
		, filled(0)
	{
	}

	void writen(uint32_t data, int bits) {
		if (filled + bits > 64) {
			store();
		}
		current |= (((uint64_t)data & ((uint64_t(1) << bits) - 1)) << (64 - filled - bits));
		filled += bits;
	}

	template <int bits>
	void write(uint32_t data) {
		writen(data, bits);
	}

	template <bool bits>
	void byteAlign() {
		int pad = ((filled + 7) & ~7) - filled;
		filled += pad;
		if (bits) {
			current |= ((uint64_t(1) << pad) - 1) << (64 - filled);
		}
	}

	void flush() {
		if (filled & 7) {
			THROW(FormatException, "�o�C�g�A���C�����Ă��܂���");
		}
		store();
	}

private:
	AutoBuffer& dst;
	uint64_t current;
	int filled;

	void storeByte() {
		dst.add(uint8_t(current >> 56));
		current <<= 8;
		filled -= 8;
	}

	void store() {
		while (filled >= 8) storeByte();
	}
};

class CRC32 {
public:
	CRC32() {
		createTable(table, 0x04C11DB7UL);
	}

	uint32_t calc(const uint8_t* data, int length, uint32_t crc) const {
		for (int i = 0; i < length; ++i) {
			crc = (crc << 8) ^ table[(crc >> 24) ^ data[i]];
		}
		return crc;
	}

	const uint32_t* getTable() const { return table; }

private:
	uint32_t table[256];

	static void createTable(uint32_t* table, uint32_t exp) {
		for (int i = 0; i < 256; ++i) {
			uint32_t crc = i << 24;
			for (int j = 0; j < 8; ++j) {
				if (crc & 0x80000000UL) {
					crc = (crc << 1) ^ exp;
				}
				else {
					crc = crc << 1;
				}
			}
			table[i] = crc;
		}
	}
};

enum AMT_LOG_LEVEL {
	AMT_LOG_DEBUG,
	AMT_LOG_INFO,
	AMT_LOG_WARN,
	AMT_LOG_ERROR
};

enum AMT_ERROR_COUNTER {
	// �s����PTS�̃t���[��
	AMT_ERR_UNKNOWN_PTS = 0,
	// PES�p�P�b�g�f�R�[�h�G���[
	AMT_ERR_DECODE_PACKET_FAILED,
	// H264�ɂ�����PTS�~�X�}�b�`
	AMT_ERR_H264_PTS_MISMATCH,
	// H264�ɂ�����t�B�[���h�z�u�G���[
	AMT_ERR_H264_UNEXPECTED_FIELD,
	// PTS���߂��Ă���
	AMT_ERR_NON_CONTINUOUS_PTS,
	// DRCS�}�b�s���O���Ȃ�
	AMT_ERR_NO_DRCS_MAP,
	// �����ŃR�[�h�G���[
	AMT_ERR_DECODE_AUDIO,
	// �G���[�̌�
	AMT_ERR_MAX,
};

const char* AMT_ERROR_NAMES[] = {
   "unknown-pts",
   "decode-packet-failed",
   "h264-pts-mismatch",
   "h264-unexpected-field",
   "non-continuous-pts",
	 "no-drcs-map",
	 "decode-audio-failed",
};

class AMTContext {
public:
	AMTContext()
		: timePrefix(true)
		, acp(GetACP())
		, errCounter()
	{ }

	const CRC32* getCRC() const {
		return &crc;
	}

	// �I�I���͕�������܂ޕ������fmt�ɓn���̂͋֎~�I�I
	// �i%���܂܂�Ă���ƌ듮�삷��̂Łj

	void debug(const char *str) const {
		print(str, AMT_LOG_DEBUG);
	}
	template <typename ... Args>
	void debugF(const char *fmt, const Args& ... args) const {
		print(StringFormat(fmt, args ...).c_str(), AMT_LOG_DEBUG);
	}
	void info(const char *str) const {
		print(str, AMT_LOG_INFO);
	}
	template <typename ... Args>
	void infoF(const char *fmt, const Args& ... args) const {
		print(StringFormat(fmt, args ...).c_str(), AMT_LOG_INFO);
	}
	void warn(const char *str) const {
		print(str, AMT_LOG_WARN);
	}
	template <typename ... Args>
	void warnF(const char *fmt, const Args& ... args) const {
		print(StringFormat(fmt, args ...).c_str(), AMT_LOG_WARN);
	}
	void error(const char *str) const {
		print(str, AMT_LOG_ERROR);
	}
	template <typename ... Args>
	void errorF(const char *fmt, const Args& ... args) const {
		print(StringFormat(fmt, args ...).c_str(), AMT_LOG_ERROR);
	}
	void progress(const char *str) const {
		printProgress(str);
	}
	template <typename ... Args>
	void progressF(const char *fmt, const Args& ... args) const {
		printProgress(StringFormat(fmt, args ...).c_str());
	}

	void registerTmpFile(const tstring& path) {
		tmpFiles.insert(path);
	}

	void clearTmpFiles() {
		for (auto& path : tmpFiles) {
			if (path.find(_T('*')) != tstring::npos) {
				auto dir = pathGetDirectory(path);
				for (auto name : GetDirectoryFiles(dir, path.substr(dir.size() + 1))) {
					auto path2 = dir + _T("/") + name;
					removeT(path2.c_str());
				}
			}
			else {
				removeT(path.c_str());
			}
		}
		tmpFiles.clear();
	}

	void incrementCounter(AMT_ERROR_COUNTER err) {
		errCounter[err]++;
	}

	int getErrorCount(AMT_ERROR_COUNTER err) const {
		return errCounter[err];
	}

	void setError(const Exception& exception) {
		errMessage = exception.message();
	}

	const std::string& getError() const {
		return errMessage;
	}
	
	void setTimePrefix(bool enable) {
		timePrefix = enable;
	}

	const std::map<std::string, std::wstring>& getDRCSMapping() const {
		return drcsMap;
	}

	void loadDRCSMapping(const tstring& mapPath)
	{
		if (File::exists(mapPath) == false) {
			THROWF(ArgumentException, "DRCS�}�b�s���O�t�@�C����������܂���: %s",
				mapPath.c_str());
		}
		else {
			// BOM����UTF-8�œǂݍ���
			std::wifstream input(mapPath);
			// codecvt_utf8 ��UCS-2�ɂ����Ή����Ă��Ȃ��̂Ŏg��Ȃ��悤�ɁI
			// �K�� codecvt_utf8_utf16 ���g������
			input.imbue(std::locale(input.getloc(), new std::codecvt_utf8_utf16<wchar_t, 0x10ffff, std::consume_header>));
			for (std::wstring line; getline(input, line);)
			{
				if (line.size() >= 34) {
					std::string key(line.begin(), line.begin() + 32);
					std::transform(key.begin(), key.end(), key.begin(), ::toupper);
					bool ok = (line[32] == '=');
					for (auto c : key) if (!isxdigit(c)) ok = false;
					if (ok) {
						drcsMap[key] = std::wstring(line.begin() + 33, line.end());
					}
				}
			}
		}
	}

	// �R���\�[���o�͂��f�t�H���g�R�[�h�y�[�W�ɐݒ�
	void setDefaultCP() {
		SetConsoleCP(acp);
		SetConsoleOutputCP(acp);
	}

private:
	bool timePrefix;
	CRC32 crc;
	int acp;

	std::set<tstring> tmpFiles;
	std::array<int, AMT_ERR_MAX> errCounter;
	std::string errMessage;

	std::map<std::string, std::wstring> drcsMap;

  void printWithTimePrefix(const char* str) const {
    time_t rawtime;
    char buffer[80];

    time(&rawtime);
    tm * timeinfo = localtime(&rawtime);

    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    PRINTF("%s %s\n", buffer, str);
  }

	void print(const char* str, AMT_LOG_LEVEL level) const {
    if (timePrefix) {
      printWithTimePrefix(str);
    }
    else {
      static const char* log_levels[] = { "debug", "info", "warn", "error" };
      PRINTF("AMT [%s] %s\n", log_levels[level], str);
    }
	}

	void printProgress(const char* str) const {
    if (timePrefix) {
      printWithTimePrefix(str);
    }
    else {
      PRINTF("AMT %s\r", str);
    }
	}
};

class AMTObject {
public:
	AMTObject(AMTContext& ctx) : ctx(ctx) { }
	virtual ~AMTObject() { }
	AMTContext& ctx;
};

enum DECODER_TYPE {
	DECODER_DEFAULT = 0,
	DECODER_QSV,
	DECODER_CUVID,
};

struct DecoderSetting {
	DECODER_TYPE mpeg2;
	DECODER_TYPE h264;
	DECODER_TYPE hevc;

	DecoderSetting()
		: mpeg2(DECODER_DEFAULT)
		, h264(DECODER_DEFAULT)
		, hevc(DECODER_DEFAULT)
	{ }
};

enum CMType {
	CMTYPE_BOTH = 0,
	CMTYPE_NONCM = 1,
	CMTYPE_CM = 2,
	CMTYPE_MAX
};

// �o�̓t�@�C������
struct EncodeFileKey {
	int video;   // ���ԃt�@�C���ԍ��i�f���t�H�[�}�b�g�؂�ւ��ɂ�镪���j
	int format;  // �t�H�[�}�b�g�ԍ��i�������̑��̃t�H�[�}�b�g�ύX�ɂ�镪���j
	int div;     // �����ԍ��iCM�\���F���ɂ�镪���j
	CMType cm;   // CM�^�C�v�i�{�ҁACM�Ȃǁj

	explicit EncodeFileKey()
		: video(0), format(0), div(0), cm(CMTYPE_BOTH) { }
	EncodeFileKey(int video, int format)
		: video(video), format(format), div(0), cm(CMTYPE_BOTH) { }
	EncodeFileKey(int video, int format, int div, CMType cm)
		: video(video), format(format), div(div), cm(cm) { }

	int key() const {
		return (video << 24) | (format << 14) | (div << 4) | cm;
	}
};

static const char* CMTypeToString(CMType cmtype) {
	if (cmtype == CMTYPE_CM) return "CM";
	if (cmtype == CMTYPE_NONCM) return "�{��";
	return "";
}

enum VIDEO_STREAM_FORMAT {
	VS_UNKNOWN,
	VS_MPEG2,
	VS_H264,
	VS_H265
};

enum PICTURE_TYPE {
	PIC_FRAME = 0, // progressive frame
	PIC_FRAME_DOUBLING, // frame doubling
	PIC_FRAME_TRIPLING, // frame tripling
	PIC_TFF, // top field first
	PIC_BFF, // bottom field first
	PIC_TFF_RFF, // tff ���� repeat first field
	PIC_BFF_RFF, // bff ���� repeat first field
	MAX_PIC_TYPE,
};

const char* PictureTypeString(PICTURE_TYPE pic) {
	switch (pic) {
	case PIC_FRAME: return "FRAME";
	case PIC_FRAME_DOUBLING: return "DBL";
	case PIC_FRAME_TRIPLING: return "TLP";
	case PIC_TFF: return "TFF";
	case PIC_BFF: return "BFF";
	case PIC_TFF_RFF: return "TFF_RFF";
	case PIC_BFF_RFF: return "BFF_RFF";
	default: return "UNK";
	}
}

enum FRAME_TYPE {
	FRAME_NO_INFO = 0,
	FRAME_I,
	FRAME_P,
	FRAME_B,
	FRAME_OTHER,
	MAX_FRAME_TYPE,
};

const char* FrameTypeString(FRAME_TYPE frame) {
	switch (frame) {
	case FRAME_I: return "I";
	case FRAME_P: return "P";
	case FRAME_B: return "B";
	default: return "UNK";
	}
}

double presenting_time(PICTURE_TYPE picType, double frameRate) {
	switch (picType) {
	case PIC_FRAME: return 1.0 / frameRate;
	case PIC_FRAME_DOUBLING: return 2.0 / frameRate;
	case PIC_FRAME_TRIPLING: return 3.0 / frameRate;
	case PIC_TFF: return 1.0 / frameRate;
	case PIC_BFF: return 1.0 / frameRate;
	case PIC_TFF_RFF: return 1.5 / frameRate;
	case PIC_BFF_RFF: return 1.5 / frameRate;
	}
	// �s��
	return 1.0 / frameRate;
}

struct VideoFormat {
	VIDEO_STREAM_FORMAT format;
	int width, height; // �t���[���̉��c
	int displayWidth, displayHeight; // �t���[���̓��\���̈�̏c���i�\���̈撆�S�I�t�Z�b�g�̓[���Ɖ���j
	int sarWidth, sarHeight; // �A�X�y�N�g��
	int frameRateNum, frameRateDenom; // �t���[�����[�g
	uint8_t colorPrimaries, transferCharacteristics, colorSpace; // �J���[�X�y�[�X
	bool progressive, fixedFrameRate;

	bool isEmpty() const {
		return width == 0;
	}

	bool isSARUnspecified() const {
		return sarWidth == 0 && sarHeight == 1;
	}

	void mulDivFps(int mul, int div) {
		frameRateNum *= mul;
		frameRateDenom *= div;
		int g = gcd(frameRateNum, frameRateDenom);
		frameRateNum /= g;
		frameRateDenom /= g;
	}

	void getDAR(int& darWidth, int& darHeight) const {
		darWidth = displayWidth * sarWidth;
		darHeight = displayHeight * sarHeight;
		int denom = gcd(darWidth, darHeight);
		darWidth /= denom;
		darHeight /= denom;
	}

	// �A�X�y�N�g��͌��Ȃ�
	bool isBasicEquals(const VideoFormat& o) const {
		return (width == o.width && height == o.height
			&& frameRateNum == o.frameRateNum && frameRateDenom == o.frameRateDenom
			&& progressive == o.progressive);
	}

	// �A�X�y�N�g�������
	bool operator==(const VideoFormat& o) const {
		return (isBasicEquals(o)
			&& displayWidth == o.displayWidth && displayHeight == o.displayHeight
			&& sarWidth == o.sarWidth && sarHeight == o.sarHeight);
	}
	bool operator!=(const VideoFormat& o) const {
		return !(*this == o);
	}

private:
	static int gcd(int u, int v) {
		int r;
		while (0 != v) {
			r = u % v;
			u = v;
			v = r;
		}
		return u;
	}
};

struct VideoFrameInfo {
	int64_t PTS, DTS;
	// MPEG2�̏ꍇ sequence header ������
	// H264�̏ꍇ SPS ������
	bool isGopStart;
	bool progressive;
	PICTURE_TYPE pic;
	FRAME_TYPE type; // �g��Ȃ����ǎQ�l���
	int codedDataSize; // �f���r�b�g�X�g���[���ɂ�����o�C�g��
	VideoFormat format;
};

enum AUDIO_CHANNELS {
	AUDIO_NONE,

	AUDIO_MONO,
	AUDIO_STEREO,
	AUDIO_30, // 3/0
	AUDIO_31, // 3/1
	AUDIO_32, // 3/2
	AUDIO_32_LFE, // 5.1ch

	AUDIO_21, // 2/1
	AUDIO_22, // 2/2
	AUDIO_2LANG, // 2 ���� (1/ 0 + 1 / 0)

				 // �ȉ�4K����
				 AUDIO_52_LFE, // 7.1ch
				 AUDIO_33_LFE, // 3/3.1
				 AUDIO_2_22_LFE, // 2/0/0-2/0/2-0.1
				 AUDIO_322_LFE, // 3/2/2.1
				 AUDIO_2_32_LFE, // 2/0/0-3/0/2-0.1
				 AUDIO_020_32_LFE, // 0/2/0-3/0/2-0.1 // AUDIO_2_32_LFE�Ƌ�ʂł��Ȃ��ˁH
				 AUDIO_2_323_2LFE, // 2/0/0-3/2/3-0.2
				 AUDIO_333_523_3_2LFE, // 22.2ch
};

const char* getAudioChannelString(AUDIO_CHANNELS channels) {
	switch (channels) {
	case AUDIO_MONO: return "���m����";
	case AUDIO_STEREO: return "�X�e���I";
	case AUDIO_30: return "3/0";
	case AUDIO_31: return "3/1";
	case AUDIO_32: return "3/2";
	case AUDIO_32_LFE: return "5.1ch";
	case AUDIO_21: return "2/1";
	case AUDIO_22: return "2/2";
	case AUDIO_2LANG: return "�f���A�����m";
	case AUDIO_52_LFE: return "7.1ch";
	case AUDIO_33_LFE: return "3/3.1";
	case AUDIO_2_22_LFE: return "2/0/0-2/0/2-0.1";
	case AUDIO_322_LFE: return "3/2/2.1";
	case AUDIO_2_32_LFE: return "2/0/0-3/0/2-0.1";
	case AUDIO_020_32_LFE: return "0/2/0-3/0/2-0.1";
	case AUDIO_2_323_2LFE: return "2/0/0-3/2/3-0.2";
	case AUDIO_333_523_3_2LFE: return "22.2ch";
	}
	return "�G���[";
}

int getNumAudioChannels(AUDIO_CHANNELS channels) {
	switch (channels) {
	case AUDIO_MONO: return 1;
	case AUDIO_STEREO: return 2;
	case AUDIO_30: return 3;
	case AUDIO_31: return 4;
	case AUDIO_32: return 5;
	case AUDIO_32_LFE: return 6;
	case AUDIO_21: return 3;
	case AUDIO_22: return 4;
	case AUDIO_2LANG: return 2;
	case AUDIO_52_LFE: return 8;
	case AUDIO_33_LFE: return 7;
	case AUDIO_2_22_LFE: return 7;
	case AUDIO_322_LFE: return 8;
	case AUDIO_2_32_LFE: return 8;
	case AUDIO_020_32_LFE: return 8;
	case AUDIO_2_323_2LFE: return 12;
	case AUDIO_333_523_3_2LFE: return 24;
	}
	return 2; // �s��
}

struct AudioFormat {
	AUDIO_CHANNELS channels;
	int sampleRate;

	bool operator==(const AudioFormat& o) const {
		return (channels == o.channels && sampleRate == o.sampleRate);
	}
	bool operator!=(const AudioFormat& o) const {
		return !(*this == o);
	}
};

struct AudioFrameInfo {
	int64_t PTS;
	int numSamples; // 1�`�����l��������̃T���v����
	AudioFormat format;
};

struct AudioFrameData : public AudioFrameInfo {
	int codedDataSize;
	uint8_t* codedData;
	int numDecodedSamples;
	int decodedDataSize;
	uint16_t* decodedData;
};

class IVideoParser {
public:
	// �Ƃ肠�����K�v�ȕ�����
	virtual void reset() = 0;

	// PTS, DTS: 90kHz�^�C���X�^���v ��񂪂Ȃ��ꍇ��-1
	virtual bool inputFrame(MemoryChunk frame, std::vector<VideoFrameInfo>& info, int64_t PTS, int64_t DTS) = 0;
};

enum NicoJKType {
	NICOJK_720S = 0,
	NICOJK_720T = 1,
	NICOJK_1080S = 2,
	NICOJK_1080T = 3,
	NICOJK_MAX
};

#include "avisynth.h"
#include "utvideo/utvideo.h"
#include "utvideo/Codec.h"

static void DeleteScriptEnvironment(IScriptEnvironment2* env) {
	if (env) env->DeleteScriptEnvironment();
}

typedef std::unique_ptr<IScriptEnvironment2, decltype(&DeleteScriptEnvironment)> ScriptEnvironmentPointer;

static ScriptEnvironmentPointer make_unique_ptr(IScriptEnvironment2* env) {
	return ScriptEnvironmentPointer(env, DeleteScriptEnvironment);
}

static void DeleteUtVideoCodec(CCodec* codec) {
	if (codec) CCodec::DeleteInstance(codec);
}

typedef std::unique_ptr<CCodec, decltype(&DeleteUtVideoCodec)> CCodecPointer;

static CCodecPointer make_unique_ptr(CCodec* codec) {
	return CCodecPointer(codec, DeleteUtVideoCodec);
}

// 1�C���X�^���X�͏�������or�ǂݍ��݂̂ǂ��炩��������g���Ȃ�
class LosslessVideoFile : AMTObject
{
	struct LosslessFileHeader {
		int magic;
		int version;
		int width;
		int height;
	};

	File file;
	LosslessFileHeader fh;
	std::vector<uint8_t> extra;
	std::vector<int> framesizes;
	std::vector<int64_t> offsets;

	int current;

public:
	LosslessVideoFile(AMTContext& ctx, const tstring& filepath, const tchar* mode)
		: AMTObject(ctx)
		, file(filepath, mode)
		, current()
	{
	}

	void writeHeader(int width, int height, int numframes, const std::vector<uint8_t>& extra)
	{
		fh.magic = 0x012345;
		fh.version = 1;
		fh.width = width;
		fh.height = height;
		framesizes.resize(numframes);
		offsets.resize(numframes);

		file.writeValue(fh);
		file.writeArray(extra);
		file.writeArray(framesizes);
		offsets[0] = file.pos();
	}

	void readHeader()
	{
		fh = file.readValue<LosslessFileHeader>();
		extra = file.readArray<uint8_t>();
		framesizes = file.readArray<int>();
		offsets.resize(framesizes.size());
		offsets[0] = file.pos();

		for (int i = 1; i < (int)framesizes.size(); ++i) {
			offsets[i] = offsets[i - 1] + framesizes[i - 1];
		}
	}

	int getWidth() const { return fh.width; }
	int getHeight() const { return fh.height; }
	int getNumFrames() const { return (int)framesizes.size(); }
	const std::vector<uint8_t>& getExtra() const { return extra; }

	void writeFrame(const uint8_t* data, int len)
	{
		int numframes = (int)framesizes.size();
		int n = current++;
		if (n >= numframes) {
			THROWF(InvalidOperationException, "[LosslessVideoFile] attempt to write frame more than specified num frames");
		}

		if (n > 0) {
			offsets[n] = offsets[n - 1] + framesizes[n - 1];
		}
		framesizes[n] = len;

		// �f�[�^����������
		file.seek(offsets[n], SEEK_SET);
		file.write(MemoryChunk((uint8_t*)data, len));

		// ��������������
		file.seek(offsets[0] - sizeof(int) * (numframes - n), SEEK_SET);
		file.writeValue(len);
	}

	int64_t readFrame(int n, uint8_t* data)
	{
		file.seek(offsets[n], SEEK_SET);
		file.read(MemoryChunk(data, framesizes[n]));
		return framesizes[n];
	}
};

static void CopyYV12(uint8_t* dst, PVideoFrame& frame, int width, int height)
{
	const uint8_t* srcY = frame->GetReadPtr(PLANAR_Y);
	const uint8_t* srcU = frame->GetReadPtr(PLANAR_U);
	const uint8_t* srcV = frame->GetReadPtr(PLANAR_V);
	int pitchY = frame->GetPitch(PLANAR_Y);
	int pitchUV = frame->GetPitch(PLANAR_U);
	int widthUV = width >> 1;
	int heightUV = height >> 1;

	uint8_t* dstp = dst;
	for (int y = 0; y < height; ++y) {
		memcpy(dstp, &srcY[y * pitchY], width);
		dstp += width;
	}
	for (int y = 0; y < heightUV; ++y) {
		memcpy(dstp, &srcU[y * pitchUV], widthUV);
		dstp += widthUV;
	}
	for (int y = 0; y < heightUV; ++y) {
		memcpy(dstp, &srcV[y * pitchUV], widthUV);
		dstp += widthUV;
	}
}

static void CopyYV12(PVideoFrame& dst, uint8_t* frame, int width, int height)
{
	uint8_t* dstY = dst->GetWritePtr(PLANAR_Y);
	uint8_t* dstU = dst->GetWritePtr(PLANAR_U);
	uint8_t* dstV = dst->GetWritePtr(PLANAR_V);
	int pitchY = dst->GetPitch(PLANAR_Y);
	int pitchUV = dst->GetPitch(PLANAR_U);
	int widthUV = width >> 1;
	int heightUV = height >> 1;

	uint8_t* srcp = frame;
	for (int y = 0; y < height; ++y) {
		memcpy(&dstY[y * pitchY], srcp, width);
		srcp += width;
	}
	for (int y = 0; y < heightUV; ++y) {
		memcpy(&dstU[y * pitchUV], srcp, widthUV);
		srcp += widthUV;
	}
	for (int y = 0; y < heightUV; ++y) {
		memcpy(&dstV[y * pitchUV], srcp, widthUV);
		srcp += widthUV;
	}
}

static void CopyYV12(uint8_t* dst,
	const uint8_t* srcY, const uint8_t* srcU, const uint8_t* srcV,
	int pitchY, int pitchUV, int width, int height)
{
	int widthUV = width >> 1;
	int heightUV = height >> 1;

	uint8_t* dstp = dst;
	for (int y = 0; y < height; ++y) {
		memcpy(dstp, &srcY[y * pitchY], width);
		dstp += width;
	}
	for (int y = 0; y < heightUV; ++y) {
		memcpy(dstp, &srcU[y * pitchUV], widthUV);
		dstp += widthUV;
	}
	for (int y = 0; y < heightUV; ++y) {
		memcpy(dstp, &srcV[y * pitchUV], widthUV);
		dstp += widthUV;
	}
}

void ConcatFiles(const std::vector<tstring>& srcpaths, const tstring& dstpath)
{
	enum { BUF_SIZE = 16 * 1024 * 1024 };
	auto buf = std::unique_ptr<uint8_t[]>(new uint8_t[BUF_SIZE]);
	File dstfile(dstpath, _T("wb"));
	for (int i = 0; i < (int)srcpaths.size(); ++i) {
		File srcfile(srcpaths[i], _T("rb"));
		while (true) {
			size_t readBytes = srcfile.read(MemoryChunk(buf.get(), BUF_SIZE));
			dstfile.write(MemoryChunk(buf.get(), readBytes));
			if (readBytes != BUF_SIZE) break;
		}
	}
}

// BOM����UTF8�ŏ�������
void WriteUTF8File(const tstring& filename, const std::string& utf8text)
{
	File file(filename, _T("w"));
	uint8_t bom[] = { 0xEF, 0xBB, 0xBF };
	file.write(MemoryChunk(bom, sizeof(bom)));
	file.write(MemoryChunk((uint8_t*)utf8text.data(), utf8text.size()));
}

void WriteUTF8File(const tstring& filename, const std::wstring& text)
{
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	WriteUTF8File(filename, converter.to_bytes(text));
}

// C API for P/Invoke
extern "C" __declspec(dllexport) AMTContext* AMTContext_Create() { return new AMTContext(); }
extern "C" __declspec(dllexport) void ATMContext_Delete(AMTContext* ptr) { delete ptr; }
extern "C" __declspec(dllexport) const char* AMTContext_GetError(AMTContext* ptr) { return ptr->getError().c_str(); }
