/**
* Amtasukaze Transcode Setting
* Copyright (c) 2017-2019 Nekopanda
*
* This software is released under the MIT License.
* http://opensource.org/licenses/mit-license.php
*/
#pragma once

#include <string>
#include <filesystem>

#include "StreamUtils.hpp"

// �J���[�X�y�[�X��`���g������
#include "libavutil/pixfmt.h"

namespace fs = std::filesystem;

struct EncoderZone {
	int startFrame;
	int endFrame;
};

struct BitrateZone : EncoderZone {
	double bitrate;

	BitrateZone()
		: EncoderZone()
		, bitrate()
	{ }
	BitrateZone(EncoderZone zone)
		: EncoderZone(zone)
		, bitrate()
	{ }
	BitrateZone(EncoderZone zone, double bitrate)
		: EncoderZone(zone)
		, bitrate(bitrate)
	{ }
};

namespace av {

// �J���[�X�y�[�X3�Z�b�g
// x265�͐��l���̂܂܂ł�OK�����Ax264��help���������string�łȂ����
// �Ȃ�Ȃ��悤�Ȃ̂ŕϊ����`
// �Ƃ肠����ARIB STD-B32 v3.7�ɏ����Ă���̂���

// 3���F
static const char* getColorPrimStr(int color_prim) {
	switch (color_prim) {
	case AVCOL_PRI_BT709: return "bt709";
	case AVCOL_PRI_BT2020: return "bt2020";
	default:
		THROWF(FormatException,
			"Unsupported color primaries (%d)", color_prim);
	}
	return NULL;
}

// �K���}
static const char* getTransferCharacteristicsStr(int transfer_characteritics) {
	switch (transfer_characteritics) {
	case AVCOL_TRC_BT709: return "bt709";
	case AVCOL_TRC_IEC61966_2_4: return "iec61966-2-4";
	case AVCOL_TRC_BT2020_10: return "bt2020-10";
	case AVCOL_TRC_SMPTEST2084: return "smpte-st-2084";
	case AVCOL_TRC_ARIB_STD_B67: return "arib-std-b67";
	default:
		THROWF(FormatException,
			"Unsupported color transfer characteritics (%d)", transfer_characteritics);
	}
	return NULL;
}

// �ϊ��W��
static const char* getColorSpaceStr(int color_space) {
	switch (color_space) {
	case AVCOL_SPC_BT709: return "bt709";
	case AVCOL_SPC_BT2020_NCL: return "bt2020nc";
	default:
		THROWF(FormatException,
			"Unsupported color color space (%d)", color_space);
	}
	return NULL;
}

} // namespace av {

enum ENUM_ENCODER {
	ENCODER_X264,
	ENCODER_X265,
	ENCODER_QSVENC,
	ENCODER_NVENC,
	ENCODER_VCEENC,
	ENCODER_SVTAV1,
};

enum ENUM_FORMAT {
	FORMAT_MP4,
	FORMAT_MKV,
	FORMAT_M2TS,
	FORMAT_TS,
};

struct BitrateSetting {
	double a, b;
	double h264;
	double h265;

	double getTargetBitrate(VIDEO_STREAM_FORMAT format, double srcBitrate) const {
		double base = a * srcBitrate + b;
		if (format == VS_H264) {
			return base * h264;
		}
		else if (format == VS_H265) {
			return base * h265;
		}
		return base;
	}
};

static const char* encoderToString(ENUM_ENCODER encoder) {
	switch (encoder) {
	case ENCODER_X264: return "x264";
	case ENCODER_X265: return "x265";
	case ENCODER_QSVENC: return "QSVEnc";
	case ENCODER_NVENC: return "NVEnc";
	case ENCODER_VCEENC: return "VCEEnc";
	case ENCODER_SVTAV1: return "SVT-AV1";
	}
	return "Unknown";
}

static tstring makeEncoderArgs(
	ENUM_ENCODER encoder,
	const tstring& binpath,
	const tstring& options,
	const VideoFormat& fmt,
	const tstring& timecodepath,
	int vfrTimingFps,
	const tstring& outpath)
{
	StringBuilderT sb;

	sb.append(_T("\"%s\""), binpath);

	// y4m�w�b�_�ɂ���̂ŕK�v�Ȃ�
	//ss << " --fps " << fmt.frameRateNum << "/" << fmt.frameRateDenom;
	//ss << " --input-res " << fmt.width << "x" << fmt.height;
	//ss << " --sar " << fmt.sarWidth << ":" << fmt.sarHeight;

	if (encoder != ENCODER_SVTAV1) {
		if (fmt.colorPrimaries != AVCOL_PRI_UNSPECIFIED) {
			sb.append(_T(" --colorprim %s"), av::getColorPrimStr(fmt.colorPrimaries));
		}
		if (fmt.transferCharacteristics != AVCOL_TRC_UNSPECIFIED) {
			sb.append(_T(" --transfer %s"), av::getTransferCharacteristicsStr(fmt.transferCharacteristics));
		}
		if (fmt.colorSpace != AVCOL_TRC_UNSPECIFIED) {
			sb.append(_T(" --colormatrix %s"), av::getColorSpaceStr(fmt.colorSpace));
		}
	}

	// �C���^�[���[�X
	switch (encoder) {
	case ENCODER_X264:
	case ENCODER_QSVENC:
	case ENCODER_NVENC:
	case ENCODER_VCEENC:
		sb.append(fmt.progressive ? _T("") : _T(" --tff"));
		break;
	case ENCODER_X265:
		//sb.append(fmt.progressive ? " --no-interlace" : " --interlace tff");
		if (fmt.progressive == false) {
			THROW(ArgumentException, "HEVC�̃C���^�[���[�X�o�͂ɂ͑Ή����Ă��܂���");
		}
		break;
	case ENCODER_SVTAV1:
		if (fmt.progressive == false) {
			THROW(ArgumentException, "AV1�̃C���^�[���[�X�o�͂ɂ͑Ή����Ă��܂���");
		}
		break;
	}

	if (encoder == ENCODER_SVTAV1) {
		sb.append(_T(" %s -b \"%s\""), options, outpath);
	}
	else {
		sb.append(_T(" %s -o \"%s\""), options, outpath);
	}

	// ���͌`��
	switch (encoder) {
	case ENCODER_X264:
		sb.append(_T(" --stitchable"))
			.append(_T(" --demuxer y4m -"));
		break;
	case ENCODER_X265:
		sb.append(_T(" --no-opt-qp-pps --no-opt-ref-list-length-pps"))
			.append(_T(" --y4m --input -"));
		break;
	case ENCODER_QSVENC:
	case ENCODER_NVENC:
	case ENCODER_VCEENC:
		sb.append(_T(" --format raw --y4m -i -"));
		break;
	case ENCODER_SVTAV1:
		sb.append(_T(" -i stdin"));
		break;
	}

	if (timecodepath.size() > 0 && encoder == ENCODER_X264) {
		std::pair<int, int> timebase = std::make_pair(fmt.frameRateNum * (vfrTimingFps / 30), fmt.frameRateDenom);
		sb.append(_T(" --tcfile-in \"%s\" --timebase %d/%d"), timecodepath, timebase.second, timebase.first);
	}

	return sb.str();
}

enum ENUM_AUDIO_ENCODER {
	AUDIO_ENCODER_NONE,
	AUDIO_ENCODER_NEROAAC,
	AUDIO_ENCODER_QAAC,
	AUDIO_ENCODER_FDKAAC,
};

static tstring makeAudioEncoderArgs(
	ENUM_AUDIO_ENCODER encoder,
	const tstring& binpath,
	const tstring& options,
	int kbps,
	const tstring& outpath)
{
	StringBuilderT sb;

	sb.append(_T("\"%s\" %s"), binpath, options);

	if (kbps) {
		switch (encoder) {
		case AUDIO_ENCODER_NEROAAC:
			sb.append(_T(" -br %d "), kbps * 1000);
			break;
		case AUDIO_ENCODER_QAAC:
			sb.append(_T(" -a %d "), kbps * 1000);
			break;
		case AUDIO_ENCODER_FDKAAC:
			sb.append(_T(" -b %d "), kbps * 1000);
			break;
		}
	}

	switch (encoder) {
	case AUDIO_ENCODER_NEROAAC:
		sb.append(_T(" -if - -of \"%s\""), outpath);
		break;
	case AUDIO_ENCODER_QAAC:
	case AUDIO_ENCODER_FDKAAC:
		sb.append(_T(" -o \"%s\" -"), outpath);
		break;
	}

	return sb.str();
}

static std::vector<std::pair<tstring, bool>> makeMuxerArgs(
	ENUM_FORMAT format,
	const tstring& binpath,
	const tstring& timelineeditorpath,
	const tstring& mp4boxpath,
	const tstring& inVideo,
	const VideoFormat& videoFormat,
	const std::vector<tstring>& inAudios,
	const tstring& outpath,
	const tstring& tmpoutpath,
	const tstring& chapterpath,
	const tstring& timecodepath,
	std::pair<int, int> timebase,
	const std::vector<tstring>& inSubs,
	const std::vector<tstring>& subsTitles,
	const tstring& metapath)
{
	std::vector<std::pair<tstring, bool>> ret;

	StringBuilderT sb;
	sb.append(_T("\"%s\""), binpath);

	if (format == FORMAT_MP4) {
		bool needChapter = (chapterpath.size() > 0);
		bool needTimecode = (timecodepath.size() > 0);
		bool needSubs = (inSubs.size() > 0);

		// �܂���muxer�ŉf���A�����A�`���v�^�[��mux
		if (videoFormat.fixedFrameRate) {
			sb.append(_T(" -i \"%s?fps=%d/%d\""), inVideo,
				videoFormat.frameRateNum, videoFormat.frameRateDenom);
		}
		else {
			sb.append(_T(" -i \"%s\""), inVideo);
		}
		for (const auto& inAudio : inAudios) {
			sb.append(_T(" -i \"%s\""), inAudio);
		}
		// timelineeditor���`���v�^�[�������̂�timecode�����鎞��mp4box�œ����
		if (needChapter && !needTimecode) {
			sb.append(_T(" --chapter \"%s\""), chapterpath);
			needChapter = false;
		}
		sb.append(_T(" --optimize-pd"));

		tstring dst = needTimecode ? tmpoutpath : outpath;
		sb.append(_T(" -o \"%s\""), dst);

		ret.push_back(std::make_pair(sb.str(), false));
		sb.clear();

		if (needTimecode) {
			// �K�v�Ȃ�timelineeditor��timecode�𖄂ߍ���
			sb.append(_T("\"%s\""), timelineeditorpath)
				.append(_T(" --track 1"))
				.append(_T(" --timecode \"%s\""), timecodepath)
				.append(_T(" --media-timescale %d"), timebase.first)
				.append(_T(" --media-timebase %d"), timebase.second)
				.append(_T(" \"%s\""), dst)
				.append(_T(" \"%s\""), outpath);
			ret.push_back(std::make_pair(sb.str(), false));
			sb.clear();
			needTimecode = false;
		}

		if (needChapter || needSubs) {
			// �����ƃ`���v�^�[�𖄂ߍ���
			sb.append(_T("\"%s\""), mp4boxpath);
			for (int i = 0; i < (int)inSubs.size(); ++i) {
				if (subsTitles[i] == _T("SRT")) { // mp4��SRT�̂�
					sb.append(_T(" -add \"%s#:name=%s\""), inSubs[i], subsTitles[i]);
				}
			}
			needSubs = false;
			// timecode������ꍇ�͂������Ń`���v�^�[������
			if (needChapter) {
				sb.append(_T(" -chap \"%s\""), chapterpath);
				needChapter = false;
			}
			sb.append(_T(" \"%s\""), outpath);
			ret.push_back(std::make_pair(sb.str(), true));
			sb.clear();
		}
	}
	else if (format == FORMAT_MKV) {

		if (chapterpath.size() > 0) {
			sb.append(_T(" --chapters \"%s\""), chapterpath);
		}

		sb.append(_T(" -o \"%s\""), outpath);

		if (timecodepath.size()) {
			sb.append(_T(" --timestamps \"0:%s\""), timecodepath);
		}
		sb.append(_T(" \"%s\""), inVideo);

		for (const auto& inAudio : inAudios) {
			sb.append(_T(" \"%s\""), inAudio);
		}
		for (int i = 0; i < (int)inSubs.size(); ++i) {
			sb.append(_T(" --track-name \"0:%s\" \"%s\""), subsTitles[i], inSubs[i]);
		}

		ret.push_back(std::make_pair(sb.str(), true));
		sb.clear();
	}
	else { // M2TS or TS
		sb.append(_T(" \"%s\" \"%s\""), metapath, outpath);
		ret.push_back(std::make_pair(sb.str(), true));
		sb.clear();
	}

	return ret;
}

static tstring makeTimelineEditorArgs(
	const tstring& binpath,
	const tstring& inpath,
	const tstring& outpath,
	const tstring& timecodepath)
{
	StringBuilderT sb;
	sb.append(_T("\"%s\""), binpath)
		.append(_T(" --track 1"))
		.append(_T(" --timecode \"%s\""), timecodepath)
		.append(_T(" \"%s\""), inpath)
		.append(_T(" \"%s\""), outpath);
	return sb.str();
}

static const char* cmOutMaskToString(int outmask) {
	switch (outmask)
	{
	case 1: return "�ʏ�";
	case 2: return "CM���J�b�g";
	case 3: return "�ʏ�o�͂�CM�J�b�g�o��";
	case 4: return "CM�̂�";
	case 5: return "�ʏ�o�͂�CM�o��";
	case 6: return "�{�҂�CM�𕪗�";
	case 7: return "�ʏ�,�{��,CM�S�o��";
	}
	return "�s��";
}

enum AMT_CLI_MODE {
	AMT_CLI_TS,
	AMT_CLI_GENERIC,
};

enum AMT_PRINT_PREFIX {
  AMT_PREFIX_DEFAULT,
  AMT_PREFIX_TIME,
};

class TempDirectory : AMTObject, NonCopyable
{
public:
	TempDirectory(AMTContext& ctx, const tstring& tmpdir, const tstring& cachedir, bool noRemoveTmp)
		: AMTObject(ctx)
		, path_(tmpdir)
		, cacheDirName_(cachedir)
		, initialized_(false)
		, noRemoveTmp_(noRemoveTmp)
	{ }
	~TempDirectory() {
		if (!initialized_ || noRemoveTmp_) {
			return;
		}
		// �ꎞ�t�@�C�����폜
		ctx.clearTmpFiles();
		// �f�B���N�g���폜
		if (rmdirT(path_.c_str()) != 0) {
			ctx.warnF("�ꎞ�f�B���N�g���폜�Ɏ��s: ", path_);
		}
	}

	void Initialize() {
		if (initialized_) return;
		if (!cacheDirName_.empty())
		{
			const fs::path srcPath = cacheDirName_;
			const auto filename = srcPath.stem();
			const auto path = StringFormat(_T("%s/amt%s"), path_, filename.wstring());
			fs::path p = path;
			if (!fs::exists(p))
			{
				if (fs::create_directory(p)) {
					path_ = path;
					initialized_ = true;
					return;
				}
				cacheDirName_.clear();
			}
			else
			{
				path_ = path;
				initialized_ = true;
				return;
			}
		}
		for (int code = (int)time(NULL) & 0xFFFFFF; code > 0; ++code) {
			auto path = genPath(path_, code);
			fs::path p = path;
			auto exist = fs::exists(p);
			if (mkdirT(path.c_str()) == 0) {
				path_ = path;
				exist = fs::exists(p);
				break;
			}
			if (errno != EEXIST) {
				break;
			}
		}
		if (path_.size() == 0) {
			THROW(IOException, "�ꎞ�f�B���N�g���쐬���s");
		}

		tstring abolutePath;
		int sz = GetFullPathNameW(path_.c_str(), 0, 0, 0);
		abolutePath.resize(sz);
		GetFullPathNameW(path_.c_str(), sz, &abolutePath[0], 0);
		abolutePath.resize(sz - 1);
		path_ = pathNormalize(abolutePath);
		initialized_ = true;
	}

	tstring path() const {
		if (!initialized_) {
			THROW(InvalidOperationException, "�ꎞ�f�B���N�g�����쐬���Ă��܂���");
		}
		return path_;
	}

	bool IsCache() const {
		return !cacheDirName_.empty();
	}

private:
	tstring path_;
	tstring cacheDirName_;
	bool initialized_;
	bool noRemoveTmp_;

	tstring genPath(const tstring& base, int code)
	{
		return StringFormat(_T("%s/amt%d"), base, code);
	}
};

static const char* GetCMSuffix(CMType cmtype) {
	switch (cmtype) {
	case CMTYPE_CM: return "-cm";
	case CMTYPE_NONCM: return "-main";
	case CMTYPE_BOTH: return "";
	}
	return "";
}

static const char* GetNicoJKSuffix(NicoJKType type) {
	switch (type) {
	case NICOJK_720S: return "-720S";
	case NICOJK_720T: return "-720T";
	case NICOJK_1080S: return "-1080S";
	case NICOJK_1080T: return "-1080T";
	}
	return "";
}

struct Config {
	// �ꎞ�t�H���_
	tstring workDir;
	tstring mode;
	tstring modeArgs; // �e�X�g�p
	// ���̓t�@�C���p�X�i�g���q���܂ށj
	tstring srcFilePath;
	// �o�̓t�@�C���p�X�i�g���q�������j
	tstring outVideoPath;
	// ���ʏ��JSON�o�̓p�X
	tstring outInfoJsonPath;
	// DRCS�}�b�s���O�t�@�C���p�X
	tstring drcsMapPath;
	tstring drcsOutPath;
	// �t�B���^�p�X
	tstring filterScriptPath;
	tstring postFilterScriptPath;
	// �G���R�[�_�ݒ�
	ENUM_ENCODER encoder;
	tstring encoderPath;
	tstring encoderOptions;
	ENUM_AUDIO_ENCODER audioEncoder;
	tstring audioEncoderPath;
	tstring audioEncoderOptions;
	tstring muxerPath;
	tstring timelineditorPath;
	tstring mp4boxPath;
	tstring nicoConvAssPath;
	tstring nicoConvChSidPath;
	ENUM_FORMAT format;
	bool usingCache;
	bool splitSub;
	bool twoPass;
	bool autoBitrate;
	bool chapter;
	bool subtitles;
	bool makeTrimavs;
	int nicojkmask;
	bool nicojk18;
	bool useNicoJKLog;
	BitrateSetting bitrate;
	double bitrateCM;
	double x265TimeFactor;
	int serviceId;
	DecoderSetting decoderSetting;
	int audioBitrateInKbps;
	int numEncodeBufferFrames;
	// CM��͗p�ݒ�
	std::vector<tstring> logoPath;
	std::vector<tstring> eraseLogoPath;
	bool ignoreNoLogo;
	bool ignoreNoDrcsMap;
	bool ignoreNicoJKError;
	double pmtCutSideRate[2];
	bool looseLogoDetection;
	bool noDelogo;
	int maxFadeLength;
	tstring chapterExePath;
	tstring chapterExeOptions;
	tstring joinLogoScpPath;
	tstring joinLogoScpCmdPath;
	tstring joinLogoScpOptions;
	int cmoutmask;
	tstring trimavsPath;
	// ���o���[�h�p
	int maxframes;
	// �z�X�g�v���Z�X�Ƃ̒ʐM�p
	HANDLE inPipe;
	HANDLE outPipe;
	int affinityGroup;
	uint64_t affinityMask;
	// �f�o�b�O�p�ݒ�
	bool dumpStreamInfo;
	bool systemAvsPlugin;
	bool noRemoveTmp;
	bool dumpFilter;
	AMT_PRINT_PREFIX printPrefix;
};

class ConfigWrapper : public AMTObject
{
public:
	ConfigWrapper(
		AMTContext& ctx,
		const Config& conf)
		: AMTObject(ctx)
		, conf(conf)
		, tmpDir(ctx, conf.workDir, conf.usingCache ? conf.srcFilePath : _T(""), conf.noRemoveTmp || (conf.mode != _T("ts") && conf.usingCache))
	{
		for (int cmtypei = 0; cmtypei < CMTYPE_MAX; ++cmtypei) {
			if (conf.cmoutmask & (1 << cmtypei)) {
				cmtypes.push_back((CMType)cmtypei);
			}
		}
		for (int nicotypei = 0; nicotypei < NICOJK_MAX; ++nicotypei) {
			if (conf.nicojkmask & (1 << nicotypei)) {
				nicojktypes.push_back((NicoJKType)nicotypei);
			}
		}
	}

	tstring getMode() const {
		return conf.mode;
	}

	tstring getModeArgs() const {
		return conf.modeArgs;
	}

	tstring getSrcFilePath() const {
		return conf.srcFilePath;
	}

	tstring getOutInfoJsonPath() const {
		return conf.outInfoJsonPath;
	}

	tstring getFilterScriptPath() const {
		return conf.filterScriptPath;
	}

	tstring getPostFilterScriptPath() const {
		return conf.postFilterScriptPath;
	}

	ENUM_ENCODER getEncoder() const {
		return conf.encoder;
	}

	tstring getEncoderPath() const {
		return conf.encoderPath;
	}

	tstring getEncoderOptions() const {
		return conf.encoderOptions;
	}

	ENUM_AUDIO_ENCODER getAudioEncoder() const {
		return conf.audioEncoder;
	}

	bool isEncodeAudio() const {
		return conf.audioEncoder != AUDIO_ENCODER_NONE;
	}

	tstring getAudioEncoderPath() const {
		return conf.audioEncoderPath;
	}

	tstring getAudioEncoderOptions() const {
		return conf.audioEncoderOptions;
	}

	ENUM_FORMAT getFormat() const {
		return conf.format;
	}

	bool isFormatVFRSupported() const {
		return conf.format != FORMAT_M2TS && conf.format != FORMAT_TS;
	}

	tstring getMuxerPath() const {
		return conf.muxerPath;
	}

	tstring getTimelineEditorPath() const {
		return conf.timelineditorPath;
	}

	tstring getMp4BoxPath() const {
		return conf.mp4boxPath;
	}

	tstring getNicoConvAssPath() const {
		return conf.nicoConvAssPath;
	}

	tstring getNicoConvChSidPath() const {
		return conf.nicoConvChSidPath;
	}

	bool isSplitSub() const {
		return conf.splitSub;
	}

	bool isTwoPass() const {
		return conf.twoPass;
	}

	bool isAutoBitrate() const {
		return conf.autoBitrate;
	}

	bool isChapterEnabled() const {
		return conf.chapter;
	}

	bool isSubtitlesEnabled() const {
		return conf.subtitles;
	}

	bool isMakeTrimavs() const {
		return conf.makeTrimavs;
	}

	bool isNicoJKEnabled() const {
		return conf.nicojkmask != 0;
	}

	bool isNicoJK18Enabled() const {
		return conf.nicojk18;
	}

	bool isUseNicoJKLog() const {
		return conf.useNicoJKLog;
	}

	int getNicoJKMask() const {
		return conf.nicojkmask;
	}

	BitrateSetting getBitrate() const {
		return conf.bitrate;
	}

	double getBitrateCM() const {
		return conf.bitrateCM;
	}

	double getX265TimeFactor() const {
		return conf.x265TimeFactor;
	}

	int getServiceId() const {
		return conf.serviceId;
	}

	DecoderSetting getDecoderSetting() const {
		return conf.decoderSetting;
	}

	int getAudioBitrateInKbps() const {
		return conf.audioBitrateInKbps;
	}

	int getNumEncodeBufferFrames() const {
		return conf.numEncodeBufferFrames;
	}

	const std::vector<tstring>& getLogoPath() const {
		return conf.logoPath;
	}

	const std::vector<tstring>& getEraseLogoPath() const {
		return conf.eraseLogoPath;
	}

	bool isIgnoreNoLogo() const {
		return conf.ignoreNoLogo;
	}

	bool isIgnoreNoDrcsMap() const {
		return conf.ignoreNoDrcsMap;
	}

	bool isIgnoreNicoJKError() const {
		return conf.ignoreNicoJKError;
	}

	bool isPmtCutEnabled() const {
		return conf.pmtCutSideRate[0] > 0 || conf.pmtCutSideRate[1] > 0;
	}

	const double* getPmtCutSideRate() const {
		return conf.pmtCutSideRate;
	}

	bool isLooseLogoDetection() const {
		return conf.looseLogoDetection;
	}

	bool isNoDelogo() const {
		return conf.noDelogo;
	}

	int getMaxFadeLength() const {
		return conf.maxFadeLength;
	}

	tstring getChapterExePath() const {
		return conf.chapterExePath;
	}

	tstring getChapterExeOptions() const {
		return conf.chapterExeOptions;
	}

	tstring getJoinLogoScpPath() const {
		return conf.joinLogoScpPath;
	}

	tstring getJoinLogoScpCmdPath() const {
		return conf.joinLogoScpCmdPath;
	}

	tstring getJoinLogoScpOptions() const {
		return conf.joinLogoScpOptions;
	}

	tstring getTrimAVSPath() const {
		return conf.trimavsPath;
	}

	const std::vector<CMType>& getCMTypes() const {
		return cmtypes;
	}

	const std::vector<NicoJKType>& getNicoJKTypes() const {
		return nicojktypes;
	}

	int getMaxFrames() const {
		return conf.maxframes;
	}

	HANDLE getInPipe() const {
		return conf.inPipe;
	}

	HANDLE getOutPipe() const {
		return conf.outPipe;
	}

	int getAffinityGroup() const {
		return conf.affinityGroup;
	}

	uint64_t getAffinityMask() const {
		return conf.affinityMask;
	}

	bool isDumpStreamInfo() const {
		return conf.dumpStreamInfo;
	}

	bool isSystemAvsPlugin() const {
		return conf.systemAvsPlugin;
	}

  AMT_PRINT_PREFIX getPrintPrefix() const {
    return conf.printPrefix;
  }

	tstring getAudioFilePath() const {
		return regtmp(StringFormat(_T("%s/audio.dat"), tmpDir.path()));
	}

	tstring getWaveFilePath() const {
		return regtmp(StringFormat(_T("%s/audio.wav"), tmpDir.path()));
	}

	tstring getIntVideoFilePath(int index) const {
		return regtmp(StringFormat(_T("%s/i%d.mpg"), tmpDir.path(), index));
	}

	tstring getPacketInfoPath() const {
		return regtmp(tmpDir.path() + _T("/packetinfo.dat"));
	}

	tstring getStreamInfoPath() const {
		return regtmp(tmpDir.path() + _T("/streaminfo.dat"));
	}

	tstring getEncVideoFilePath(EncodeFileKey key) const {
		return regtmp(StringFormat(_T("%s/v%d-%d-%d%s.raw"),
			tmpDir.path(), key.video, key.format, key.div, GetCMSuffix(key.cm)));
	}

	tstring getAfsTimecodePath(EncodeFileKey key) const {
		return regtmp(StringFormat(_T("%s/v%d-%d-%d%s.timecode.txt"),
			tmpDir.path(), key.video, key.format, key.div, GetCMSuffix(key.cm)));
	}

	tstring getAvsTmpPath(EncodeFileKey key) const {
		auto str = StringFormat(_T("%s/v%d-%d-%d%s.avstmp"),
			tmpDir.path(), key.video, key.format, key.div, GetCMSuffix(key.cm));
		ctx.registerTmpFile(str + _T("*"));
		return str;
	}

	tstring getAvsDurationPath(EncodeFileKey key) const {
		return regtmp(StringFormat(_T("%s/v%d-%d-%d%s.avstmp"),
			tmpDir.path(), key.video, key.format, key.div, GetCMSuffix(key.cm)) + _T(".duration.txt"));
	}

	tstring getAvsTimecodePath(EncodeFileKey key) const {
		return regtmp(StringFormat(_T("%s/v%d-%d-%d%s.avstmp"),
			tmpDir.path(), key.video, key.format, key.div, GetCMSuffix(key.cm)) + _T(".timecode.txt"));
	}

	tstring getFilterAvsPath(EncodeFileKey key) const {
		auto str = StringFormat(_T("%s/vfilter%d-%d-%d%s.avs"), 
			tmpDir.path(), key.video, key.format, key.div, GetCMSuffix(key.cm));
		ctx.registerTmpFile(str);
		return str;
	}

	tstring getEncStatsFilePath(EncodeFileKey key) const
	{
		auto str = StringFormat(_T("%s/s%d-%d-%d%s.log"), 
			tmpDir.path(), key.video, key.format, key.div, GetCMSuffix(key.cm));
		ctx.registerTmpFile(str);
		// x264��.mbtree����������̂�
		ctx.registerTmpFile(str + _T(".mbtree"));
		// x265��.cutree����������̂�
		ctx.registerTmpFile(str + _T(".cutree"));
		return str;
	}

	tstring getIntAudioFilePath(EncodeFileKey key, int aindex) const {
		return regtmp(StringFormat(_T("%s/a%d-%d-%d-%d%s.aac"),
			tmpDir.path(), key.video, key.format, key.div, aindex, GetCMSuffix(key.cm)));
	}

	tstring getTmpASSFilePath(EncodeFileKey key, int langindex) const {
		return regtmp(StringFormat(_T("%s/c%d-%d-%d-%d%s.ass"),
			tmpDir.path(), key.video, key.format, key.div, langindex, GetCMSuffix(key.cm)));
	}

	tstring getTmpSRTFilePath(EncodeFileKey key, int langindex) const {
		return regtmp(StringFormat(_T("%s/c%d-%d-%d-%d%s.srt"),
			tmpDir.path(), key.video, key.format, key.div, langindex, GetCMSuffix(key.cm)));
	}

	tstring getTmpBestLogoPath(int vindex) const {
		return regtmp(StringFormat(_T("%s/best_logo%d.dat"), tmpDir.path(), vindex));
	}

	tstring getTmpAMTSourcePath(int vindex) const {
		return regtmp(StringFormat(_T("%s/amts%d.dat"), tmpDir.path(), vindex));
	}

	tstring getTmpSourceAVSPath(int vindex) const {
		return regtmp(StringFormat(_T("%s/amts%d.avs"), tmpDir.path(), vindex));
	}

	tstring getTmpLogoFramePath(int vindex, int logoIndex = -1) const {
		if (logoIndex == -1) {
			return regtmp(StringFormat(_T("%s/logof%d.txt"), tmpDir.path(), vindex));
		}
		return regtmp(StringFormat(_T("%s/logof%d-%d.txt"), tmpDir.path(), vindex, logoIndex));
	}

	tstring getTmpChapterExePath(int vindex) const {
		return regtmp(StringFormat(_T("%s/chapter_exe%d.txt"), tmpDir.path(), vindex));
	}

	tstring getTmpChapterExeOutPath(int vindex) const {
		return regtmp(StringFormat(_T("%s/chapter_exe_o%d.txt"), tmpDir.path(), vindex));
	}

	tstring getTmpTrimAVSPath(int vindex) const {
		return regtmp(StringFormat(_T("%s/trim%d.avs"), tmpDir.path(), vindex));
	}

	tstring getTmpJlsPath(int vindex) const {
		return regtmp(StringFormat(_T("%s/jls%d.txt"), tmpDir.path(), vindex));
	}

	tstring getTmpDivPath(int vindex) const {
		return regtmp(StringFormat(_T("%s/div%d.txt"), tmpDir.path(), vindex));
	}

	tstring getTmpChapterPath(EncodeFileKey key) const {
		return regtmp(StringFormat(_T("%s/chapter%d-%d-%d%s.txt"),
			tmpDir.path(), key.video, key.format, key.div, GetCMSuffix(key.cm)));
	}

	tstring getTmpNicoJKXMLPath() const {
		return regtmp(StringFormat(_T("%s/nicojk.xml"), tmpDir.path()));
	}

	tstring getTmpNicoJKASSPath(NicoJKType type) const {
		return regtmp(StringFormat(_T("%s/nicojk%s.ass"), tmpDir.path(), GetNicoJKSuffix(type)));
	}

	tstring getTmpNicoJKASSPath(EncodeFileKey key, NicoJKType type) const {
		return regtmp(StringFormat(_T("%s/nicojk%d-%d-%d%s%s.ass"),
			tmpDir.path(), key.video, key.format, key.div, GetCMSuffix(key.cm), GetNicoJKSuffix(type)));
	}

	tstring getVfrTmpFilePath(EncodeFileKey key) const {
		return regtmp(StringFormat(_T("%s/t%d-%d-%d%s.%s"),
			tmpDir.path(), key.video, key.format, key.div, GetCMSuffix(key.cm), getOutputExtention()));
	}

	tstring getM2tsMetaFilePath(EncodeFileKey key) const {
		return regtmp(StringFormat(_T("%s/t%d-%d-%d%s.meta"), 
			tmpDir.path(), key.video, key.format, key.div, GetCMSuffix(key.cm)));
	}

	const char* getOutputExtention() const {
		switch (conf.format) {
		case FORMAT_MP4: return "mp4";
		case FORMAT_MKV: return "mkv";
		case FORMAT_M2TS: return "m2ts";
		case FORMAT_TS: return "ts";
		}
		return "amatsukze";
	}

	tstring getOutFilePath(EncodeFileKey key, EncodeFileKey keyMax) const {
		StringBuilderT sb;
		sb.append(_T("%s"), conf.outVideoPath);
		if (key.format > 0) {
			sb.append(_T("-%d"), key.format);
		}
		if (keyMax.div > 1) {
			sb.append(_T("_div%d"), key.div + 1);
		}
		sb.append(_T("%s.%s"), GetCMSuffix(key.cm), getOutputExtention());
		return sb.str();
	}

	tstring getOutASSPath(EncodeFileKey key, EncodeFileKey keyMax, int langidx, NicoJKType jktype) const {
		StringBuilderT sb;
		sb.append(_T("%s"), conf.outVideoPath);
		if (key.format > 0) {
			sb.append(_T("-%d"), key.format);
		}
		if (keyMax.div > 1) {
			sb.append(_T("_div%d"), key.div + 1);
		}
		sb.append(_T("%s"), GetCMSuffix(key.cm));
		if (langidx < 0) {
			sb.append(_T("-nicojk%s"), GetNicoJKSuffix(jktype));
		}
		else if (langidx > 0) {
			sb.append(_T("-%d"), langidx);
		}
		sb.append(_T(".ass"));
		return sb.str();
	}

	tstring getOutSummaryPath() const {
		return StringFormat(_T("%s.txt"), conf.outVideoPath);
	}

	tstring getDRCSMapPath() const {
		return conf.drcsMapPath;
	}

	tstring getDRCSOutPath(const std::string& md5) const {
		return StringFormat(_T("%s/%s.bmp"), conf.drcsOutPath, md5);
	}

	bool isDumpFilter() const {
		return conf.dumpFilter;
	}

	tstring getFilterGraphDumpPath(EncodeFileKey key) const {
		return regtmp(StringFormat(_T("%s/graph%d-%d-%d%s.txt"),
			tmpDir.path(), key.video, key.format, key.div, GetCMSuffix(key.cm)));
	}

	bool isZoneAvailable() const {
		return conf.encoder == ENCODER_X264 || conf.encoder == ENCODER_X265 || conf.encoder == ENCODER_NVENC;
	}

	bool isZoneWithoutBitrateAvailable() const {
		return conf.encoder == ENCODER_X264 || conf.encoder == ENCODER_X265;
	}

	bool isEncoderSupportVFR() const {
		return conf.encoder == ENCODER_X264;
	}

	bool isBitrateCMEnabled() const {
		return conf.bitrateCM != 1.0;
	}

	tstring getOptions(
		int numFrames,
		VIDEO_STREAM_FORMAT srcFormat, double srcBitrate, bool pulldown,
		int pass, const std::vector<BitrateZone>& zones, double vfrBitrateScale,
		EncodeFileKey key) const
	{
		StringBuilderT sb;
		sb.append(_T("%s"), conf.encoderOptions);
		double targetBitrate = 0;
		if (conf.autoBitrate) {
			targetBitrate = conf.bitrate.getTargetBitrate(srcFormat, srcBitrate);
			if (isEncoderSupportVFR() == false) {
				// �^�C���R�[�h��Ή��G���R�[�_�ɂ�����r�b�g���[�g��VFR����
				targetBitrate *= vfrBitrateScale;
			}
			if (key.cm == CMTYPE_CM && !isZoneAvailable()) {
				targetBitrate *= conf.bitrateCM;
			}
			double maxBitrate = std::max(targetBitrate * 2, srcBitrate);
			if (conf.encoder == ENCODER_QSVENC) {
				sb.append(_T(" --la %d --maxbitrate %d"), (int)targetBitrate, (int)maxBitrate);
			}
			else if (conf.encoder == ENCODER_NVENC) {
				sb.append(_T(" --vbrhq %d --maxbitrate %d"), (int)targetBitrate, (int)maxBitrate);
			}
			else if (conf.encoder == ENCODER_VCEENC) {
				sb.append(_T(" --vbr %d --max-bitrate %d"), (int)targetBitrate, (int)maxBitrate);
			}
			else if (conf.encoder == ENCODER_SVTAV1) {
				sb.append(_T(" -rc 2 -tbr %d"), (int)(targetBitrate * 1000));
			}
			else {
				sb.append(_T(" --bitrate %d --vbv-maxrate %d --vbv-bufsize %d"),
					(int)targetBitrate, (int)maxBitrate, (int)maxBitrate);
			}
		}
		if (pass >= 0) {
			sb.append(_T(" --pass %d --stats \"%s\""),
				pass, getEncStatsFilePath(key));
		}
		if (zones.size() &&
			isZoneAvailable() &&
			(isEncoderSupportVFR() == false || isBitrateCMEnabled()))
		{
			if (isZoneWithoutBitrateAvailable()) {
				// x264/265
				sb.append(_T(" --zones "));
				for (int i = 0; i < (int)zones.size(); ++i) {
					auto zone = zones[i];
					sb.append(_T("%s%d,%d,b=%.3g"), (i > 0) ? "/" : "",
						zone.startFrame, zone.endFrame - 1, zone.bitrate);
				}
			}
			else if(conf.autoBitrate) {
				// NVEnc
				for (int i = 0; i < (int)zones.size(); ++i) {
					auto zone = zones[i];
					sb.append(_T(" --dynamic-rc %d:%d,vbrhq=%d"), 
						zone.startFrame, zone.endFrame - 1, (int)std::round(targetBitrate * zone.bitrate));
				}
			}
		}
		if (numFrames > 0) {
			if (conf.encoder == ENCODER_X264 || conf.encoder == ENCODER_X265) {
				sb.append(_T(" --frames %d"), numFrames);
			}
			else if (conf.encoder == ENCODER_SVTAV1) {
				sb.append(_T(" -n %d"), numFrames);
			}
		}
		return sb.str();
	}

	void dump() const {
		ctx.info("[�ݒ�]");
		if (conf.mode != _T("ts")) {
			ctx.infoF("Mode: %s", conf.mode);
		}
		ctx.infoF("����: %s", conf.srcFilePath);
		ctx.infoF("�o��: %s", conf.outVideoPath);
		ctx.infoF("�ꎞ�t�H���_: %s", tmpDir.path());
		ctx.infoF("�o�̓t�H�[�}�b�g: %s", formatToString(conf.format));
		ctx.infoF("�G���R�[�_: %s (%s)", conf.encoderPath, encoderToString(conf.encoder));
		ctx.infoF("�G���R�[�_�I�v�V����: %s", conf.encoderOptions);
		if (conf.autoBitrate) {
			ctx.infoF("�����r�b�g���[�g: �L�� (%g:%g:%g)",
				conf.bitrate.a, conf.bitrate.b, conf.bitrate.h264);
		}
		else {
			ctx.info("�����r�b�g���[�g: ����");
		}
		ctx.infoF("�G���R�[�h/�o��: %s/%s",
			conf.twoPass ? "2�p�X" : "1�p�X",
			cmOutMaskToString(conf.cmoutmask));
		ctx.infoF("�`���v�^�[���: %s%s",
			conf.chapter ? "�L��" : "����",
			(conf.chapter && conf.ignoreNoLogo) ? "" : "�i���S�K�{�j");
		if (conf.chapter) {
			for (int i = 0; i < (int)conf.logoPath.size(); ++i) {
				ctx.infoF("logo%d: %s", (i + 1), conf.logoPath[i]);
			}
			ctx.infoF("���S����: %s", conf.noDelogo ? "���Ȃ�" : "����");
		}
		ctx.infoF("����: %s", conf.subtitles ? "�L��" : "����");
		if (conf.subtitles) {
			ctx.infoF("DRCS�}�b�s���O: %s", conf.drcsMapPath);
		}
		if (conf.mode == _T("cm")) {
			ctx.infoF("trim.avs�t�@�C���쐬: %s", conf.makeTrimavs ? "����" : "���Ȃ�");
		}
		if (conf.serviceId > 0) {
			ctx.infoF("�T�[�r�XID: %d", conf.serviceId);
		}
		else {
			ctx.info("�T�[�r�XID: �w��Ȃ�");
		}
		ctx.infoF("�f�R�[�_: MPEG2:%s H264:%s",
			decoderToString(conf.decoderSetting.mpeg2),
			decoderToString(conf.decoderSetting.h264));
	}

	void CreateTempDir() {
		tmpDir.Initialize();
	}

	bool IsUsingCache() const {
		return tmpDir.IsCache();
	}

private:
	Config conf;
	TempDirectory tmpDir;
	std::vector<CMType> cmtypes;
	std::vector<NicoJKType> nicojktypes;

	const char* decoderToString(DECODER_TYPE decoder) const {
		switch (decoder) {
		case DECODER_QSV: return "QSV";
		case DECODER_CUVID: return "CUVID";
		}
		return "default";
	}

	const char* formatToString(ENUM_FORMAT fmt) const {
		switch (fmt) {
		case FORMAT_MP4: return "MP4";
		case FORMAT_MKV: return "Matroska";
		case FORMAT_M2TS: return "M2TS";
		case FORMAT_TS: return "TS";
		}
		return "unknown";
	}

	tstring regtmp(tstring str) const {
		ctx.registerTmpFile(str);
		return str;
	}
};
