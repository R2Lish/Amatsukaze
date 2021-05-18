/**
* Transcode manager
* Copyright (c) 2017-2019 Nekopanda
*
* This software is released under the MIT License.
* http://opensource.org/licenses/mit-license.php
*/
#pragma once

#include <string>
#include <memory>
#include <limits>
#include <smmintrin.h>

#include "TsSplitter.hpp"
#include "Encoder.hpp"
#include "Muxer.hpp"
#include "StreamReform.hpp"
#include "LogoScan.hpp"
#include "CMAnalyze.hpp"
#include "InterProcessComm.hpp"
#include "CaptionData.hpp"
#include "CaptionFormatter.hpp"
#include "EncoderOptionParser.hpp"
#include "NicoJK.hpp"
#include "AudioEncoder.hpp"

class AMTSplitter : public TsSplitter {
public:
	class PacketInfo
	{
	public:
		PacketInfo()
		{}
		PacketInfo(const AMTSplitter* splitter)
			:selectedServiceId(splitter->getActualServiceId())
			, numTotalPackets(splitter->getNumTotalPackets())
			, numScramblePackets(splitter->getNumScramblePackets())
			, totalIntVideoSize_(splitter->getTotalIntVideoSize())
			, srcFileSize_(splitter->getSrcFileSize())
		{}

		int selectedServiceId;
		int64_t numTotalPackets;
		int64_t numScramblePackets;
		int64_t totalIntVideoSize_;
		int64_t srcFileSize_;

		void serialize(const tstring& path) const
		{
			const auto& file = File(path, _T("wb"));
			file.writeValue(selectedServiceId);
			file.writeValue(numTotalPackets);
			file.writeValue(numScramblePackets);
			file.writeValue(totalIntVideoSize_);
			file.writeValue(srcFileSize_);
		}
		static PacketInfo desrialize(const tstring& path)
		{
			PacketInfo si;
			const auto& file = File(path, _T("rb"));
			si.selectedServiceId = file.readValue<int>();
			si.numTotalPackets = file.readValue<int64_t>();
			si.numScramblePackets = file.readValue<int64_t>();
			si.totalIntVideoSize_ = file.readValue<int64_t>();
			si.srcFileSize_ = file.readValue<int64_t>();
			return si;
		}
	};

	AMTSplitter(AMTContext& ctx, const ConfigWrapper& setting)
		: TsSplitter(ctx, true, true, setting.isSubtitlesEnabled())
		, setting_(setting)
		, psWriter(ctx)
		, writeHandler(*this)
		, audioFile_(setting.getAudioFilePath(), _T("wb"))
		, waveFile_(setting.getWaveFilePath(), _T("wb"))
		, curVideoFormat_()
		, videoFileCount_(0)
		, videoStreamType_(-1)
		, audioStreamType_(-1)
		, audioFileSize_(0)
		, waveFileSize_(0)
		, srcFileSize_(0)
	{
		psWriter.setHandler(&writeHandler);
	}

	StreamReformInfo split()
	{
		readAll();

		// for debug
		printInteraceCount();

		return StreamReformInfo(ctx, videoFileCount_,
			videoFrameList_, audioFrameList_, captionTextList_, streamEventList_, timeList_);
	}

	int64_t getSrcFileSize() const {
		return srcFileSize_;
	}

	int64_t getTotalIntVideoSize() const {
		return writeHandler.getTotalSize();
	}

	PacketInfo getPacketInfo() const {
		return PacketInfo(this);
	}

	void serialize(const tstring& path) const 
	{
		getPacketInfo().serialize(path);
	}

	static PacketInfo deserialize(const tstring& path) {
		return PacketInfo::desrialize(path);
	}

protected:
	class StreamFileWriteHandler : public PsStreamWriter::EventHandler {
		TsSplitter& this_;
		std::unique_ptr<File> file_;
		int64_t totalIntVideoSize_;
	public:
		StreamFileWriteHandler(TsSplitter& this_)
			: this_(this_), totalIntVideoSize_() { }
		virtual void onStreamData(MemoryChunk mc) {
			if (file_ != NULL) {
				file_->write(mc);
				totalIntVideoSize_ += mc.length;
			}
		}
		void open(const tstring& path) {
			totalIntVideoSize_ = 0;
			file_ = std::unique_ptr<File>(new File(path, _T("wb")));
		}
		void close() {
			file_ = nullptr;
		}
		int64_t getTotalSize() const {
			return totalIntVideoSize_;
		}
	};

	const ConfigWrapper& setting_;
	PsStreamWriter psWriter;
	StreamFileWriteHandler writeHandler;
	File audioFile_;
	File waveFile_;
	VideoFormat curVideoFormat_;

	int videoFileCount_;
	int videoStreamType_;
	int audioStreamType_;
	int64_t audioFileSize_;
	int64_t waveFileSize_;
	int64_t srcFileSize_;

	// �f�[�^
	std::vector<FileVideoFrameInfo> videoFrameList_;
	std::vector<FileAudioFrameInfo> audioFrameList_;
	std::vector<StreamEvent> streamEventList_;
	std::vector<CaptionItem> captionTextList_;
	std::vector<std::pair<int64_t, JSTTime>> timeList_;

	void readAll() {
		enum { BUFSIZE = 4 * 1024 * 1024 };
		auto buffer_ptr = std::unique_ptr<uint8_t[]>(new uint8_t[BUFSIZE]);
		MemoryChunk buffer(buffer_ptr.get(), BUFSIZE);
		File srcfile(setting_.getSrcFilePath(), _T("rb"));
		srcFileSize_ = srcfile.size();
		size_t readBytes;
		do {
			readBytes = srcfile.read(buffer);
			inputTsData(MemoryChunk(buffer.data, readBytes));
		} while (readBytes == buffer.length);
	}

	static bool CheckPullDown(PICTURE_TYPE p0, PICTURE_TYPE p1) {
		switch (p0) {
		case PIC_TFF:
		case PIC_BFF_RFF:
			return (p1 == PIC_TFF || p1 == PIC_TFF_RFF);
		case PIC_BFF:
		case PIC_TFF_RFF:
			return (p1 == PIC_BFF || p1 == PIC_BFF_RFF);
		default: // ����ȊO�̓`�F�b�N�ΏۊO
			return true;
		}
	}

	void printInteraceCount() {

		if (videoFrameList_.size() == 0) {
			ctx.error("�t���[��������܂���");
			return;
		}

		// ���b�v�A���E���h���Ȃ�PTS�𐶐�
		std::vector<std::pair<int64_t, int>> modifiedPTS;
		int64_t videoBasePTS = videoFrameList_[0].PTS;
		int64_t prevPTS = videoFrameList_[0].PTS;
		for (int i = 0; i < int(videoFrameList_.size()); ++i) {
			int64_t PTS = videoFrameList_[i].PTS;
			int64_t modPTS = prevPTS + int64_t((int32_t(PTS) - int32_t(prevPTS)));
			modifiedPTS.emplace_back(modPTS, i);
			prevPTS = modPTS;
		}

		// PTS�Ń\�[�g
		std::sort(modifiedPTS.begin(), modifiedPTS.end());

#if 0
		// �t���[�����X�g���o��
		FILE* framesfp = fopen("frames.txt", "w");
		fprintf(framesfp, "FrameNumber,DecodeFrameNumber,PTS,Duration,FRAME_TYPE,PIC_TYPE,IsGOPStart\n");
		for (int i = 0; i < (int)modifiedPTS.size(); ++i) {
			int64_t PTS = modifiedPTS[i].first;
			int decodeIndex = modifiedPTS[i].second;
			const VideoFrameInfo& frame = videoFrameList_[decodeIndex];
			int PTSdiff = -1;
			if (i < (int)modifiedPTS.size() - 1) {
				int64_t nextPTS = modifiedPTS[i + 1].first;
				const VideoFrameInfo& nextFrame = videoFrameList_[modifiedPTS[i + 1].second];
				PTSdiff = int(nextPTS - PTS);
				if (CheckPullDown(frame.pic, nextFrame.pic) == false) {
					ctx.warn("Flag Check Error: PTS=%lld %s -> %s",
						PTS, PictureTypeString(frame.pic), PictureTypeString(nextFrame.pic));
				}
			}
			fprintf(framesfp, "%d,%d,%lld,%d,%s,%s,%d\n",
				i, decodeIndex, PTS, PTSdiff, FrameTypeString(frame.type), PictureTypeString(frame.pic), frame.isGopStart ? 1 : 0);
		}
		fclose(framesfp);
#endif

		// PTS�Ԋu���o��
		struct Integer {
			int v;
			Integer() : v(0) { }
		};

		std::array<int, MAX_PIC_TYPE> interaceCounter = { 0 };
		std::map<int, Integer> PTSdiffMap;
		prevPTS = -1;
		for (const auto& ptsIndex : modifiedPTS) {
			int64_t PTS = ptsIndex.first;
			const VideoFrameInfo& frame = videoFrameList_[ptsIndex.second];
			interaceCounter[(int)frame.pic]++;
			if (prevPTS != -1) {
				int PTSdiff = int(PTS - prevPTS);
				PTSdiffMap[PTSdiff].v++;
			}
			prevPTS = PTS;
		}

		ctx.info("[�f���t���[�����v���]");

		int64_t totalTime = modifiedPTS.back().first - videoBasePTS;
		double sec = (double)totalTime / MPEG_CLOCK_HZ;
		int minutes = (int)(sec / 60);
		sec -= minutes * 60;
		ctx.infoF("����: %d��%.3f�b", minutes, sec);

		ctx.infoF("FRAME=%d DBL=%d TLP=%d TFF=%d BFF=%d TFF_RFF=%d BFF_RFF=%d",
			interaceCounter[0], interaceCounter[1], interaceCounter[2], interaceCounter[3], interaceCounter[4], interaceCounter[5], interaceCounter[6]);

		for (const auto& pair : PTSdiffMap) {
			ctx.infoF("(PTS_Diff,Cnt)=(%d,%d)", pair.first, pair.second.v);
		}
	}

	// TsSplitter���z�֐� //

	virtual void onVideoPesPacket(
		int64_t clock,
		const std::vector<VideoFrameInfo>& frames,
		PESPacket packet)
	{
		for (const VideoFrameInfo& frame : frames) {
			videoFrameList_.push_back(frame);
			videoFrameList_.back().fileOffset = writeHandler.getTotalSize();
		}
		psWriter.outVideoPesPacket(clock, frames, packet);
	}

	virtual void onVideoFormatChanged(VideoFormat fmt) {
		ctx.info("[�f���t�H�[�}�b�g�ύX]");

		StringBuilder sb;
		sb.append("�T�C�Y: %dx%d", fmt.width, fmt.height);
		if (fmt.width != fmt.displayWidth || fmt.height != fmt.displayHeight) {
			sb.append(" �\���̈�: %dx%d", fmt.displayWidth, fmt.displayHeight);
		}
		int darW, darH; fmt.getDAR(darW, darH);
		sb.append(" (%d:%d)", darW, darH);
		if (fmt.fixedFrameRate) {
			sb.append(" FPS: %d/%d", fmt.frameRateNum, fmt.frameRateDenom);
		}
		else {
			sb.append(" FPS: VFR");
		}
		ctx.info(sb.str().c_str());

		// �t�@�C���ύX
		if (!curVideoFormat_.isBasicEquals(fmt)) {
			// �A�X�y�N�g��ȊO���ύX����Ă�����t�@�C���𕪂���
			//�iStreamReform�Ə��������킹�Ȃ���΂Ȃ�Ȃ����Ƃɒ��Ӂj
			writeHandler.open(setting_.getIntVideoFilePath(videoFileCount_++));
			psWriter.outHeader(videoStreamType_, audioStreamType_);
		}
		curVideoFormat_ = fmt;

		StreamEvent ev = StreamEvent();
		ev.type = VIDEO_FORMAT_CHANGED;
		ev.frameIdx = (int)videoFrameList_.size();
		streamEventList_.push_back(ev);
	}

	virtual void onAudioPesPacket(
		int audioIdx,
		int64_t clock,
		const std::vector<AudioFrameData>& frames,
		PESPacket packet)
	{
		for (const AudioFrameData& frame : frames) {
			FileAudioFrameInfo info = frame;
			info.audioIdx = audioIdx;
			info.codedDataSize = frame.codedDataSize;
			info.waveDataSize = frame.decodedDataSize;
			info.fileOffset = audioFileSize_;
			info.waveOffset = waveFileSize_;
			audioFile_.write(MemoryChunk(frame.codedData, frame.codedDataSize));
			if (frame.decodedDataSize > 0) {
				waveFile_.write(MemoryChunk((uint8_t*)frame.decodedData, frame.decodedDataSize));
			}
			audioFileSize_ += frame.codedDataSize;
			waveFileSize_ += frame.decodedDataSize;
			audioFrameList_.push_back(info);
		}
		if (videoFileCount_ > 0) {
			psWriter.outAudioPesPacket(audioIdx, clock, frames, packet);
		}
	}

	virtual void onAudioFormatChanged(int audioIdx, AudioFormat fmt) {
		ctx.infoF("[����%d�t�H�[�}�b�g�ύX]", audioIdx);
		ctx.infoF("�`�����l��: %s �T���v�����[�g: %d",
			getAudioChannelString(fmt.channels), fmt.sampleRate);

		StreamEvent ev = StreamEvent();
		ev.type = AUDIO_FORMAT_CHANGED;
		ev.audioIdx = audioIdx;
		ev.frameIdx = (int)audioFrameList_.size();
		streamEventList_.push_back(ev);
	}

	virtual void onCaptionPesPacket(
		int64_t clock,
		std::vector<CaptionItem>& captions,
		PESPacket packet)
	{
		for (auto& caption : captions) {
			captionTextList_.emplace_back(std::move(caption));
		}
	}

	virtual DRCSOutInfo getDRCSOutPath(int64_t PTS, const std::string& md5) {
		DRCSOutInfo info;
		info.elapsed = (videoFrameList_.size() > 0) ? (double)(PTS - videoFrameList_[0].PTS) : -1.0;
		info.filename = setting_.getDRCSOutPath(md5);
		return info;
	}

	// TsPacketSelectorHandler���z�֐� //

	virtual void onPidTableChanged(const PMTESInfo video, const std::vector<PMTESInfo>& audio, const PMTESInfo caption) {
		// �x�[�X�N���X�̏���
		TsSplitter::onPidTableChanged(video, audio, caption);

		ASSERT(audio.size() > 0);
		videoStreamType_ = video.stype;
		audioStreamType_ = audio[0].stype;

		StreamEvent ev = StreamEvent();
		ev.type = PID_TABLE_CHANGED;
		ev.numAudio = (int)audio.size();
		ev.frameIdx = (int)videoFrameList_.size();
		streamEventList_.push_back(ev);
	}

	virtual void onTime(int64_t clock, JSTTime time) {
		timeList_.push_back(std::make_pair(clock, time));
	}
};

class EncoderArgumentGenerator
{
public:
	EncoderArgumentGenerator(
		const ConfigWrapper& setting,
		StreamReformInfo& reformInfo)
		: setting_(setting)
		, reformInfo_(reformInfo)
	{ }

	tstring GenEncoderOptions(
		int numFrames,
		VideoFormat outfmt,
		std::vector<BitrateZone> zones,
		double vfrBitrateScale,
		tstring timecodepath,
		int vfrTimingFps,
		EncodeFileKey key, int pass)
	{
		VIDEO_STREAM_FORMAT srcFormat = reformInfo_.getVideoStreamFormat();
		double srcBitrate = getSourceBitrate(key.video);
		return makeEncoderArgs(
			setting_.getEncoder(),
			setting_.getEncoderPath(),
			setting_.getOptions(
				numFrames,
				srcFormat, srcBitrate, false, pass, zones, vfrBitrateScale, key),
			outfmt,
			timecodepath,
			vfrTimingFps,
			setting_.getEncVideoFilePath(key));
	}

	// src, target
	std::pair<double, double> printBitrate(AMTContext& ctx, EncodeFileKey key) const
	{
		double srcBitrate = getSourceBitrate(key.video);
		ctx.infoF("���͉f���r�b�g���[�g: %d kbps", (int)srcBitrate);
		VIDEO_STREAM_FORMAT srcFormat = reformInfo_.getVideoStreamFormat();
		double targetBitrate = std::numeric_limits<float>::quiet_NaN();
		if (setting_.isAutoBitrate()) {
			targetBitrate = setting_.getBitrate().getTargetBitrate(srcFormat, srcBitrate);
			if (key.cm == CMTYPE_CM) {
				targetBitrate *= setting_.getBitrateCM();
			}
			ctx.infoF("�ڕW�f���r�b�g���[�g: %d kbps", (int)targetBitrate);
		}
		return std::make_pair(srcBitrate, targetBitrate);
	}

private:
	const ConfigWrapper& setting_;
	const StreamReformInfo& reformInfo_;

	double getSourceBitrate(int fileId) const
	{
		// �r�b�g���[�g�v�Z
		const auto& info = reformInfo_.getSrcVideoInfo(fileId);
		return ((double)info.first * 8 / 1000) / ((double)info.second / MPEG_CLOCK_HZ);
	}
};

static std::vector<BitrateZone> MakeBitrateZones(
	const std::vector<double>& timeCodes,
	const std::vector<EncoderZone>& cmzones,
	const ConfigWrapper& setting,
	VideoInfo outvi)
{
	std::vector<BitrateZone> bitrateZones;
	if (timeCodes.size() == 0 || setting.isEncoderSupportVFR()) {
		// VFR�łȂ��A�܂��́A�G���R�[�_��VFR���T�|�[�g���Ă��� -> VFR�p�ɒ�������K�v���Ȃ�
		for (int i = 0; i < (int)cmzones.size(); ++i) {
			bitrateZones.emplace_back(cmzones[i], setting.getBitrateCM());
		}
	}
	else {
		if (setting.isZoneAvailable()) {
			// VFR��Ή��G���R�[�_�Ń]�[���ɑΉ����Ă���΃r�b�g���[�g�]�[������
#if 0
			{
				File dump("zone_param.dat", "wb");
				dump.writeArray(frameDurations);
				dump.writeArray(cmzones);
				dump.writeValue(setting.getBitrateCM());
				dump.writeValue(outvi.fps_numerator);
				dump.writeValue(outvi.fps_denominator);
				dump.writeValue(setting.getX265TimeFactor());
				dump.writeValue(0.05);
			}
#endif
			return MakeVFRBitrateZones(
				timeCodes, cmzones, setting.getBitrateCM(),
				outvi.fps_numerator, outvi.fps_denominator,
				setting.getX265TimeFactor(), 0.05); // �S�̂�5%�܂ł̍��Ȃ狖�e����
		}
	}
	return bitrateZones;
}

#if 0
// �y�[�W�q�[�v���@�\���Ă��邩�e�X�g
void DoBadThing() {
	char *p = (char*)HeapAlloc(
		GetProcessHeap(),
		HEAP_GENERATE_EXCEPTIONS | HEAP_ZERO_MEMORY,
		8);
	memset(p, 'x', 32);
}
#endif

static void transcodeMain(AMTContext& ctx, const ConfigWrapper& setting)
{
#if 0
	MessageBox(NULL, "Debug", "Amatsukaze", MB_OK);
	//DoBadThing();
#endif

	const_cast<ConfigWrapper&>(setting).CreateTempDir();
	setting.dump();

	bool isNoEncode = (setting.getMode() == _T("cm"));

	auto eoInfo = ParseEncoderOption(setting.getEncoder(), setting.getEncoderOptions());
	PrintEncoderInfo(ctx, eoInfo);

	// �`�F�b�N
	if (!isNoEncode && !setting.isFormatVFRSupported() && eoInfo.afsTimecode) {
		THROW(FormatException, "M2TS/TS�o�͂�VFR���T�|�[�g���Ă��܂���");
	}
	if (!isNoEncode && eoInfo.selectEvery > 1 && eoInfo.afsTimecode) {
		THROW(FormatException, "NVEncC�̎����t�B�[���h�V�t�g(--vpp-afs timecode=true)�ɂ��VFR����"
			"�t���[���Ԉ���(--vpp-select-every)�̓����g�p�̓T�|�[�g���Ă��܂���");
	}

	ResourceManger rm(ctx, setting.getInPipe(), setting.getOutPipe());
	rm.wait(HOST_CMD_TSAnalyze);

	Stopwatch sw;
	sw.start();

	std::unique_ptr<AMTSplitter> splitter;
	bool UsePackeInfoCache = setting.IsUsingCache() && fs::exists(setting.getPacketInfoPath()) && fs::exists(setting.getStreamInfoPath());
	if (!UsePackeInfoCache) {
		splitter = std::unique_ptr<AMTSplitter>(new AMTSplitter(ctx, setting));
		if (setting.getServiceId() > 0) {
			splitter->setServiceId(setting.getServiceId());
		}
	}
	StreamReformInfo reformInfo = UsePackeInfoCache ? StreamReformInfo::deserialize(ctx, setting.getStreamInfoPath()) : splitter->split();
	const AMTSplitter::PacketInfo packetInfo = UsePackeInfoCache ? AMTSplitter::deserialize(setting.getPacketInfoPath()) : splitter->getPacketInfo();

	ctx.infoF("TS��͊���: %.2f�b", sw.getAndReset());
	int serviceId = packetInfo.selectedServiceId;
	int64_t numTotalPackets = packetInfo.numTotalPackets;
	int64_t numScramblePackets = packetInfo.numScramblePackets;
	int64_t totalIntVideoSize = packetInfo.totalIntVideoSize_;
	int64_t srcFileSize = packetInfo.srcFileSize_;

	if (setting.isDumpStreamInfo() || setting.IsUsingCache()) {
		if (!UsePackeInfoCache) {
			splitter->serialize(setting.getPacketInfoPath());
			reformInfo.serialize(setting.getStreamInfoPath());
		}
	}

	splitter = nullptr;

	// �X�N�����u���p�P�b�g�`�F�b�N
	double scrambleRatio = (double)numScramblePackets / (double)numTotalPackets;
	if (scrambleRatio > 0.01) {
		ctx.errorF("%.2f%%�̃p�P�b�g���X�N�����u����Ԃł��B", scrambleRatio * 100);
		if (scrambleRatio > 0.3) {
			THROW(FormatException, "�X�N�����u���p�P�b�g���������܂�");
		}
	}

	if (!isNoEncode && setting.isIgnoreNoDrcsMap() == false) {
		// DRCS�}�b�s���O�`�F�b�N
		if (ctx.getErrorCount(AMT_ERR_NO_DRCS_MAP) > 0) {
			THROW(NoDrcsMapException, "�}�b�s���O�ɂȂ�DRCS�O�����萳��Ɏ��������ł��Ȃ��������ߏI�����܂�");
		}
	}

	reformInfo.prepare(setting.isSplitSub(), setting.isEncodeAudio());

	time_t startTime = reformInfo.getFirstFrameTime();

	NicoJK nicoJK(ctx, setting);
	bool nicoOK = false;
	if (!isNoEncode && setting.isNicoJKEnabled()) {
		ctx.info("[�j�R�j�R�����R�����g�擾]");
		auto srcDuration = reformInfo.getInDuration() / MPEG_CLOCK_HZ;
		nicoOK = nicoJK.makeASS(serviceId, startTime, (int)srcDuration);
		if (nicoOK) {
			reformInfo.SetNicoJKList(nicoJK.getDialogues());
		}
		else {
			if (nicoJK.isFail() == false) {
				ctx.info("�Ή��`�����l��������܂���");
			}
			else if (setting.isIgnoreNicoJKError() == false) {
				THROW(RuntimeException, "�j�R�j�R�����R�����g�擾�Ɏ��s");
			}
		}
	}

	int numVideoFiles = reformInfo.getNumVideoFile();
	int mainFileIndex = reformInfo.getMainVideoFileIndex();
	std::vector<std::unique_ptr<CMAnalyze>> cmanalyze;

	// �\�[�X�t�@�C���ǂݍ��ݗp�f�[�^�ۑ�
	for (int videoFileIndex = 0; videoFileIndex < numVideoFiles; ++videoFileIndex) {
		// �t�@�C���ǂݍ��ݏ���ۑ�
		auto& fmt = reformInfo.getFormat(EncodeFileKey(videoFileIndex, 0));
		auto amtsPath = setting.getTmpAMTSourcePath(videoFileIndex);
		av::SaveAMTSource(amtsPath,
			setting.getIntVideoFilePath(videoFileIndex),
			setting.getWaveFilePath(),
			fmt.videoFormat, fmt.audioFormat[0],
			reformInfo.getFilterSourceFrames(videoFileIndex),
			reformInfo.getFilterSourceAudioFrames(videoFileIndex),
			setting.getDecoderSetting());
	}

	// ���S�ECM���
	rm.wait(HOST_CMD_CMAnalyze);
	sw.start();
	std::vector<std::pair<size_t, bool>> logoFound;
	std::vector<std::unique_ptr<MakeChapter>> chapterMakers(numVideoFiles);
	for (int videoFileIndex = 0; videoFileIndex < numVideoFiles; ++videoFileIndex) {
		size_t numFrames = reformInfo.getFilterSourceFrames(videoFileIndex).size();
		// �`���v�^�[��͂�300�t���[���i��10�b�j�ȏ゠��ꍇ����
		//�i�Z������ƃG���[�ɂȂ邱�Ƃ�����̂Łj
		bool isAnalyze = (setting.isChapterEnabled() && numFrames >= 300);

		cmanalyze.emplace_back(std::unique_ptr<CMAnalyze>(isAnalyze 
			? new CMAnalyze(ctx, setting, videoFileIndex, numFrames) 
			: new CMAnalyze(ctx, setting)));

		CMAnalyze* cma = cmanalyze.back().get();

		if (isAnalyze && setting.isPmtCutEnabled()) {
			// PMT�ύX�ɂ��CM�ǉ��F��
			cma->applyPmtCut(numFrames, setting.getPmtCutSideRate(),
				reformInfo.getPidChangedList(videoFileIndex));
		}

		if (videoFileIndex == mainFileIndex) {
			if (setting.getTrimAVSPath().size()) {
				// Trim������
				cma->inputTrimAVS(numFrames, setting.getTrimAVSPath());
			}
		}

		logoFound.emplace_back(numFrames, cma->getLogoPath().size() > 0);
		reformInfo.applyCMZones(videoFileIndex, cma->getZones(), cma->getDivs());

		if (isAnalyze) {
			chapterMakers[videoFileIndex] = std::unique_ptr<MakeChapter>(
				new MakeChapter(ctx, setting, reformInfo, videoFileIndex, cma->getTrims()));
		}
	}
	if (setting.isChapterEnabled()) {
		// ���S�����������`�F�b�N //
		// �f���t�@�C�����t���[�����Ń\�[�g
		std::sort(logoFound.begin(), logoFound.end());
		if (setting.getLogoPath().size() > 0 && // ���S�w�肠��
			setting.isIgnoreNoLogo() == false &&          // ���S�Ȃ������łȂ�
			logoFound.back().first >= 300 &&
			logoFound.back().second == false)     // �ł������f���Ń��S��������Ȃ�����
		{
			THROW(NoLogoException, "�}�b�`���郍�S��������܂���ł���");
		}
		ctx.infoF("���S�ECM��͊���: %.2f�b", sw.getAndReset());
	}

	if (isNoEncode) {
		// CM��݂͂̂Ȃ炱���ŏI��
		return;
	}

	auto audioDiffInfo = reformInfo.genAudio(setting.getCMTypes());
	audioDiffInfo.printAudioPtsDiff(ctx);

	const auto& allKeys = reformInfo.getOutFileKeys();
	std::vector<EncodeFileKey> keys;
	// 1�b�ȉ��Ȃ�o�͂��Ȃ�
	std::copy_if(allKeys.begin(), allKeys.end(), std::back_inserter(keys),
		[&](EncodeFileKey key) { return reformInfo.getEncodeFile(key).duration >= MPEG_CLOCK_HZ; });

	std::vector<EncodeFileOutput> outFileInfo(keys.size());

	ctx.info("[�`���v�^�[����]");
	for (int i = 0; i < (int)keys.size(); ++i) {
		auto key = keys[i];
		if (chapterMakers[key.video]) {
			chapterMakers[key.video]->exec(key);
		}
	}

	ctx.info("[�����t�@�C������]");
	for (int i = 0; i < (int)keys.size(); ++i) {
		auto key = keys[i];
		CaptionASSFormatter formatterASS(ctx);
		CaptionSRTFormatter formatterSRT(ctx);
		NicoJKFormatter formatterNicoJK(ctx);
		const auto& capList = reformInfo.getEncodeFile(key).captionList;
		for (int lang = 0; lang < capList.size(); ++lang) {
			auto ass = formatterASS.generate(capList[lang]);
			auto srt = formatterSRT.generate(capList[lang]);
			WriteUTF8File(setting.getTmpASSFilePath(key, lang), ass);
			if (srt.size() > 0) {
				// SRT��CP_STR_SMALL�����Ȃ������ꍇ�ȂǏo�͂��Ȃ��ꍇ������A
				// ��t�@�C����mux���ɃG���[�ɂȂ�̂ŁA1�s���Ȃ��ꍇ�͏o�͂��Ȃ�
				WriteUTF8File(setting.getTmpSRTFilePath(key, lang), srt);
			}
		}
		if (nicoOK) {
			const auto& headerLines = nicoJK.getHeaderLines();
			const auto& dialogues = reformInfo.getEncodeFile(key).nicojkList;
			for (NicoJKType jktype : setting.getNicoJKTypes()) {
				File file(setting.getTmpNicoJKASSPath(key, jktype), _T("w"));
				auto text = formatterNicoJK.generate(headerLines[(int)jktype], dialogues[(int)jktype]);
				file.write(MemoryChunk((uint8_t*)text.data(), text.size()));
			}
		}
	}
	ctx.infoF("�����t�@�C����������: %.2f�b", sw.getAndReset());

	if (setting.isEncodeAudio()) {
		ctx.info("[�����G���R�[�h]");
		for (int i = 0; i < (int)keys.size(); ++i) {
			auto key = keys[i];
			auto outpath = setting.getIntAudioFilePath(key, 0);
			auto args = makeAudioEncoderArgs(
				setting.getAudioEncoder(),
				setting.getAudioEncoderPath(),
				setting.getAudioEncoderOptions(),
				setting.getAudioBitrateInKbps(),
				outpath);
			auto format = reformInfo.getFormat(key);
			auto audioFrames = reformInfo.getWaveInput(reformInfo.getEncodeFile(key).audioFrames[0]);
			EncodeAudio(ctx, args, setting.getWaveFilePath(), format.audioFormat[0], audioFrames);
		}
	}

	auto argGen = std::unique_ptr<EncoderArgumentGenerator>(new EncoderArgumentGenerator(setting, reformInfo));

	sw.start();
	for (int i = 0; i < (int)keys.size(); ++i) {
		auto key = keys[i];
		auto& fileOut = outFileInfo[i];
		const CMAnalyze* cma = cmanalyze[key.video].get();

		AMTFilterSource filterSource(ctx, setting, reformInfo,
			cma->getZones(), cma->getLogoPath(), key, rm);

		try {
			PClip filterClip = filterSource.getClip();
			IScriptEnvironment2* env = filterSource.getEnv();
			auto encoderZones = filterSource.getZones();
			auto& outfmt = filterSource.getFormat();
			auto& outvi = filterClip->GetVideoInfo();
			auto& timeCodes = filterSource.getTimeCodes();

			ctx.infoF("[�G���R�[�h�J�n] %d/%d %s", i + 1, (int)keys.size(), CMTypeToString(key.cm));
			auto bitrate = argGen->printBitrate(ctx, key);

			fileOut.vfmt = outfmt;
			fileOut.srcBitrate = bitrate.first;
			fileOut.targetBitrate = bitrate.second;
			fileOut.vfrTimingFps = filterSource.getVfrTimingFps();

			if (timeCodes.size() > 0) {
				// �t�B���^�ɂ��VFR���L��
				if (eoInfo.afsTimecode) {
					THROW(ArgumentException, "�G���R�[�_�ƃt�B���^�̗�����VFR�^�C���R�[�h���o�͂���Ă��܂��B");
				}
				if (eoInfo.selectEvery > 1) {
					THROW(ArgumentException, "VFR�ŏo�͂���ꍇ�́A�G���R�[�_�ŊԈ������Ƃ͂ł��܂���");
				}
				else if (!setting.isFormatVFRSupported()) {
					THROW(FormatException, "M2TS/TS�o�͂�VFR���T�|�[�g���Ă��܂���");
				}
				ctx.infoF("VFR�^�C�~���O: %d fps", fileOut.vfrTimingFps);
				fileOut.timecode = setting.getAvsTimecodePath(key);
			}
			else if (eoInfo.afsTimecode) {
				fileOut.vfrTimingFps = 120;
				fileOut.timecode = setting.getAfsTimecodePath(key);
			}

			std::vector<int> pass;
			if (setting.isTwoPass()) {
				pass.push_back(1);
				pass.push_back(2);
			}
			else {
				pass.push_back(-1);
			}

			auto bitrateZones = MakeBitrateZones(timeCodes, encoderZones, setting, outvi);
			auto vfrBitrateScale = AdjustVFRBitrate(timeCodes, outvi.fps_numerator, outvi.fps_denominator);
			// VFR�t���[���^�C�~���O��120fps��
			std::vector<tstring> encoderArgs;
			for (int i = 0; i < (int)pass.size(); ++i) {
				encoderArgs.push_back(
					argGen->GenEncoderOptions(
						outvi.num_frames,
						outfmt, bitrateZones, vfrBitrateScale,
						fileOut.timecode, fileOut.vfrTimingFps, key, pass[i]));
			}
			AMTFilterVideoEncoder encoder(ctx, std::max(4, setting.getNumEncodeBufferFrames()));
			encoder.encode(filterClip, outfmt,
				timeCodes, encoderArgs, env);
		}
		catch (const AvisynthError& avserror) {
			THROWF(AviSynthException, "%s", avserror.msg);
		}
	}
	ctx.infoF("�G���R�[�h����: %.2f�b", sw.getAndReset());

	argGen = nullptr;

	rm.wait(HOST_CMD_Mux);
	sw.start();
	int64_t totalOutSize = 0;
	auto muxer = std::unique_ptr<AMTMuxder>(new AMTMuxder(ctx, setting, reformInfo));
	for (int i = 0; i < (int)keys.size(); ++i) {
		auto key = keys[i];

		ctx.infoF("[Mux�J�n] %d/%d %s", i + 1, (int)keys.size(), CMTypeToString(key.cm));
		muxer->mux(key, eoInfo, nicoOK, outFileInfo[i]);

		totalOutSize += outFileInfo[i].fileSize;
	}
	ctx.infoF("Mux����: %.2f�b", sw.getAndReset());

	muxer = nullptr;

	// �o�͌��ʂ�\��
	reformInfo.printOutputMapping([&](EncodeFileKey key) {
		const auto& file = reformInfo.getEncodeFile(key);
		return setting.getOutFilePath(file.outKey, file.keyMax);
	});

	// �o�͌���JSON�o��
	if (setting.getOutInfoJsonPath().size() > 0) {
		StringBuilder sb;
		sb.append("{ ")
			.append("\"srcpath\": \"%s\", ", toJsonString(setting.getSrcFilePath()))
			.append("\"outfiles\": [");
		for (int i = 0; i < (int)keys.size(); ++i) {
			if (i > 0) sb.append(", ");
			const auto& file = reformInfo.getEncodeFile(keys[i]);
			const auto& info = outFileInfo[i];
			sb.append("{ \"path\": \"%s\", \"srcbitrate\": %d, \"outbitrate\": %d, \"outfilesize\": %lld, ",
				toJsonString(setting.getOutFilePath(file.outKey, file.keyMax)), (int)info.srcBitrate,
				std::isnan(info.targetBitrate) ? -1 : (int)info.targetBitrate, info.fileSize);
			sb.append("\"subs\": [");
			for (int s = 0; s < (int)info.outSubs.size(); ++s) {
				if (s > 0) sb.append(", ");
				sb.append("\"%s\"", toJsonString(info.outSubs[s]));
			}
			sb.append("] }");
		}
		sb.append("]")
			.append(", \"logofiles\": [");
		for (int i = 0; i < reformInfo.getNumVideoFile(); ++i) {
			if (i > 0) sb.append(", ");
			sb.append("\"%s\"", toJsonString(cmanalyze[i]->getLogoPath()));
		}
		sb.append("]")
			.append(", \"srcfilesize\": %lld, \"intvideofilesize\": %lld, \"outfilesize\": %lld",
				srcFileSize, totalIntVideoSize, totalOutSize);
		auto duration = reformInfo.getInOutDuration();
		sb.append(", \"srcduration\": %.3f, \"outduration\": %.3f",
			(double)duration.first / MPEG_CLOCK_HZ, (double)duration.second / MPEG_CLOCK_HZ);
		sb.append(", \"audiodiff\": ");
		audioDiffInfo.printToJson(sb);
		sb.append(", \"error\": {");
		for (int i = 0; i < AMT_ERR_MAX; ++i) {
			if (i > 0) sb.append(", ");
			sb.append("\"%s\": %d", AMT_ERROR_NAMES[i], ctx.getErrorCount((AMT_ERROR_COUNTER)i));
		}
		sb.append(" }");
		sb.append(", \"cmanalyze\": %s", (setting.isChapterEnabled() ? "true" : "false"))
			.append(", \"nicojk\": %s", (nicoOK ? "true" : "false"))
			.append(", \"trimavs\": %s", (setting.getTrimAVSPath().size() ? "true" : "false"))
			.append(" }");

		std::string str = sb.str();
		MemoryChunk mc(reinterpret_cast<uint8_t*>(const_cast<char*>(str.data())), str.size());
		File file(setting.getOutInfoJsonPath(), _T("w"));
		file.write(mc);
	}
}

static void transcodeSimpleMain(AMTContext& ctx, const ConfigWrapper& setting)
{
	if (ends_with(setting.getSrcFilePath(), _T(".ts"))) {
		ctx.warn("��ʃt�@�C�����[�h�ł�TS�t�@�C���̏����͔񐄏��ł�");
	}

	auto encoder = std::unique_ptr<AMTSimpleVideoEncoder>(new AMTSimpleVideoEncoder(ctx, setting));
	encoder->encode();
	int audioCount = encoder->getAudioCount();
	int64_t srcFileSize = encoder->getSrcFileSize();
	VideoFormat videoFormat = encoder->getVideoFormat();
	encoder = nullptr;

	auto muxer = std::unique_ptr<AMTSimpleMuxder>(new AMTSimpleMuxder(ctx, setting));
	muxer->mux(videoFormat, audioCount);
	int64_t totalOutSize = muxer->getTotalOutSize();
	muxer = nullptr;

	// �o�͌��ʂ�\��
	ctx.info("����");
	if (setting.getOutInfoJsonPath().size() > 0) {
		StringBuilder sb;
		sb.append("{ \"srcpath\": \"%s\"", toJsonString(setting.getSrcFilePath()))
			.append(", \"outpath\": \"%s\"", toJsonString(setting.getOutFilePath(EncodeFileKey(), EncodeFileKey())))
			.append(", \"srcfilesize\": %lld", srcFileSize)
			.append(", \"outfilesize\": %lld", totalOutSize)
			.append(" }");

		std::string str = sb.str();
		MemoryChunk mc(reinterpret_cast<uint8_t*>(const_cast<char*>(str.data())), str.size());
		File file(setting.getOutInfoJsonPath(), _T("w"));
		file.write(mc);
	}
}


class DrcsSearchSplitter : public TsSplitter {
public:
	DrcsSearchSplitter(AMTContext& ctx, const ConfigWrapper& setting)
		: TsSplitter(ctx, true, false, true)
		, setting_(setting)
	{ }

	void readAll()
	{
		enum { BUFSIZE = 4 * 1024 * 1024 };
		auto buffer_ptr = std::unique_ptr<uint8_t[]>(new uint8_t[BUFSIZE]);
		MemoryChunk buffer(buffer_ptr.get(), BUFSIZE);
		File srcfile(setting_.getSrcFilePath(), _T("rb"));
		size_t readBytes;
		do {
			readBytes = srcfile.read(buffer);
			inputTsData(MemoryChunk(buffer.data, readBytes));
		} while (readBytes == buffer.length);
	}

protected:
	const ConfigWrapper& setting_;
	std::vector<VideoFrameInfo> videoFrameList_;

	// TsSplitter���z�֐� //

	virtual void onVideoPesPacket(
		int64_t clock,
		const std::vector<VideoFrameInfo>& frames,
		PESPacket packet)
	{
		// ���̏��ŏ��̃t���[�������K�v�Ȃ�����
		for (const VideoFrameInfo& frame : frames) {
			videoFrameList_.push_back(frame);
		}
	}

	virtual void onVideoFormatChanged(VideoFormat fmt) { }

	virtual void onAudioPesPacket(
		int audioIdx,
		int64_t clock,
		const std::vector<AudioFrameData>& frames,
		PESPacket packet)
	{ }

	virtual void onAudioFormatChanged(int audioIdx, AudioFormat fmt) { }

	virtual void onCaptionPesPacket(
		int64_t clock,
		std::vector<CaptionItem>& captions,
		PESPacket packet)
	{ }

	virtual DRCSOutInfo getDRCSOutPath(int64_t PTS, const std::string& md5) {
		DRCSOutInfo info;
		info.elapsed = (videoFrameList_.size() > 0) ? (double)(PTS - videoFrameList_[0].PTS) : -1.0;
		info.filename = setting_.getDRCSOutPath(md5);
		return info;
	}

	virtual void onTime(int64_t clock, JSTTime time) { }
};

class SubtitleDetectorSplitter : public TsSplitter {
public:
	SubtitleDetectorSplitter(AMTContext& ctx, const ConfigWrapper& setting)
		: TsSplitter(ctx, true, false, true)
		, setting_(setting)
		, hasSubtltle_(false)
	{ }

	void readAll(int maxframes)
	{
		enum { BUFSIZE = 4 * 1024 * 1024 };
		auto buffer_ptr = std::unique_ptr<uint8_t[]>(new uint8_t[BUFSIZE]);
		MemoryChunk buffer(buffer_ptr.get(), BUFSIZE);
		File srcfile(setting_.getSrcFilePath(), _T("rb"));
		auto fileSize = srcfile.size();
		// �t�@�C���擪����10%�̂Ƃ��납��ǂ�
		srcfile.seek(fileSize / 10, SEEK_SET);
		int64_t totalRead = 0;
		// �Ō��10%�͓ǂ܂Ȃ�
		int64_t end = fileSize / 10 * 9;
		size_t readBytes;
		do {
			readBytes = srcfile.read(buffer);
			inputTsData(MemoryChunk(buffer.data, readBytes));
			totalRead += readBytes;
		} while (totalRead < end && !hasSubtltle_ && videoFrameList_.size() < maxframes);
	}

	bool getHasSubtitle() const {
		return hasSubtltle_;
	}

protected:
	const ConfigWrapper& setting_;
	std::vector<VideoFrameInfo> videoFrameList_;
	bool hasSubtltle_;

	// TsSplitter���z�֐� //

	virtual void onVideoPesPacket(
		int64_t clock,
		const std::vector<VideoFrameInfo>& frames,
		PESPacket packet)
	{
		// ���̏��ŏ��̃t���[�������K�v�Ȃ�����
		for (const VideoFrameInfo& frame : frames) {
			videoFrameList_.push_back(frame);
		}
	}

	virtual void onVideoFormatChanged(VideoFormat fmt) { }

	virtual void onAudioPesPacket(
		int audioIdx,
		int64_t clock,
		const std::vector<AudioFrameData>& frames,
		PESPacket packet)
	{ }

	virtual void onAudioFormatChanged(int audioIdx, AudioFormat fmt) { }

	virtual void onCaptionPesPacket(
		int64_t clock,
		std::vector<CaptionItem>& captions,
		PESPacket packet)
	{ }

	virtual DRCSOutInfo getDRCSOutPath(int64_t PTS, const std::string& md5) {
		return DRCSOutInfo();
	}

	virtual void onTime(int64_t clock, JSTTime time) { }

	virtual void onCaptionPacket(int64_t clock, TsPacket packet) {
		hasSubtltle_ = true;
	}
};

class AudioDetectorSplitter : public TsSplitter {
public:
	AudioDetectorSplitter(AMTContext& ctx, const ConfigWrapper& setting)
		: TsSplitter(ctx, true, true, false)
		, setting_(setting)
	{ }

	void readAll(int maxframes)
	{
		enum { BUFSIZE = 4 * 1024 * 1024 };
		auto buffer_ptr = std::unique_ptr<uint8_t[]>(new uint8_t[BUFSIZE]);
		MemoryChunk buffer(buffer_ptr.get(), BUFSIZE);
		File srcfile(setting_.getSrcFilePath(), _T("rb"));
		auto fileSize = srcfile.size();
		// �t�@�C���擪����10%�̂Ƃ��납��ǂ�
		srcfile.seek(fileSize / 10, SEEK_SET);
		int64_t totalRead = 0;
		// �Ō��10%�͓ǂ܂Ȃ�
		int64_t end = fileSize / 10 * 9;
		size_t readBytes;
		do {
			readBytes = srcfile.read(buffer);
			inputTsData(MemoryChunk(buffer.data, readBytes));
			totalRead += readBytes;
		} while (totalRead < end && videoFrameList_.size() < maxframes);
	}

protected:
	const ConfigWrapper& setting_;
	std::vector<VideoFrameInfo> videoFrameList_;

	// TsSplitter���z�֐� //

	virtual void onVideoPesPacket(
		int64_t clock,
		const std::vector<VideoFrameInfo>& frames,
		PESPacket packet)
	{
		// ���̏��ŏ��̃t���[�������K�v�Ȃ�����
		for (const VideoFrameInfo& frame : frames) {
			videoFrameList_.push_back(frame);
		}
	}

	virtual void onVideoFormatChanged(VideoFormat fmt) { }

	virtual void onAudioPesPacket(
		int audioIdx,
		int64_t clock,
		const std::vector<AudioFrameData>& frames,
		PESPacket packet)
	{ }

	virtual void onAudioFormatChanged(int audioIdx, AudioFormat fmt) {
		printf("�C���f�b�N�X: %d �`�����l��: %s �T���v�����[�g: %d\n",
			audioIdx, getAudioChannelString(fmt.channels), fmt.sampleRate);
	}

	virtual void onCaptionPesPacket(
		int64_t clock,
		std::vector<CaptionItem>& captions,
		PESPacket packet)
	{ }

	virtual DRCSOutInfo getDRCSOutPath(int64_t PTS, const std::string& md5) {
		return DRCSOutInfo();
	}

	virtual void onTime(int64_t clock, JSTTime time) { }
};

static void searchDrcsMain(AMTContext& ctx, const ConfigWrapper& setting)
{
	Stopwatch sw;
	sw.start();
	auto splitter = std::unique_ptr<DrcsSearchSplitter>(new DrcsSearchSplitter(ctx, setting));
	if (setting.getServiceId() > 0) {
		splitter->setServiceId(setting.getServiceId());
	}
	splitter->readAll();
	ctx.infoF("����: %.2f�b", sw.getAndReset());
}

static void detectSubtitleMain(AMTContext& ctx, const ConfigWrapper& setting)
{
	auto splitter = std::unique_ptr<SubtitleDetectorSplitter>(new SubtitleDetectorSplitter(ctx, setting));
	if (setting.getServiceId() > 0) {
		splitter->setServiceId(setting.getServiceId());
	}
	splitter->readAll(setting.getMaxFrames());
	printf("����%s\n", splitter->getHasSubtitle() ? "����" : "�Ȃ�");
}

static void detectAudioMain(AMTContext& ctx, const ConfigWrapper& setting)
{
	auto splitter = std::unique_ptr<AudioDetectorSplitter>(new AudioDetectorSplitter(ctx, setting));
	if (setting.getServiceId() > 0) {
		splitter->setServiceId(setting.getServiceId());
	}
	splitter->readAll(setting.getMaxFrames());
}
