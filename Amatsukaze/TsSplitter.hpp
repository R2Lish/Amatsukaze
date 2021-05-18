/**
* MPEG2-TS splitter
* Copyright (c) 2017-2019 Nekopanda
*
* This software is released under the MIT License.
* http://opensource.org/licenses/mit-license.php
*/
#pragma once

#include "common.h"

#include <algorithm>
#include <vector>
#include <map>
#include <array>

#include "StreamUtils.hpp"
#include "Mpeg2TsParser.hpp"
#include "Mpeg2VideoParser.hpp"
#include "H264VideoParser.hpp"
#include "AdtsParser.hpp"
#include "Mpeg2PsWriter.hpp"
#include "WaveWriter.h"
#include "CaptionDef.h"
#include "Caption.h"
#include "CaptionData.hpp"

class VideoFrameParser : public AMTObject, public PesParser {
public:
	VideoFrameParser(AMTContext&ctx)
		: AMTObject(ctx)
		, PesParser()
		, videoStreamFormat(VS_MPEG2)
		, videoFormat()
		, mpeg2parser(ctx)
		, h264parser(ctx)
		, parser(&mpeg2parser)
	{ }

	void setStreamFormat(VIDEO_STREAM_FORMAT streamFormat) {
		if (videoStreamFormat != streamFormat) {
			switch (streamFormat) {
			case VS_MPEG2:
				parser = &mpeg2parser;
				break;
			case VS_H264:
				parser = &h264parser;
				break;
			}
			reset();
			videoStreamFormat = streamFormat;
		}
	}

	VIDEO_STREAM_FORMAT getStreamFormat() { return videoStreamFormat; }

	void reset() {
		videoFormat = VideoFormat();
		parser->reset();
	}

protected:

	virtual void onPesPacket(int64_t clock, PESPacket packet) {
		if (!packet.has_PTS()) {
			ctx.error("Video PES Packet �� PTS ������܂���");
			return;
		}

		int64_t PTS = packet.has_PTS() ? packet.PTS : -1;
		int64_t DTS = packet.has_DTS() ? packet.DTS : PTS;
		MemoryChunk payload = packet.paylod();

		if (!parser->inputFrame(payload, frameInfo, PTS, DTS)) {
			ctx.errorF("�t���[�����̎擾�Ɏ��s PTS=%lld", PTS);
			return;
		}

		if (frameInfo.size() > 0) {
			const VideoFrameInfo& frame = frameInfo[0];

			if (frame.format.isEmpty()) {
				// �t�H�[�}�b�g���킩��Ȃ��ƃf�R�[�h�ł��Ȃ��̂ŗ����Ȃ�
				return;
			}

			if (frame.format != videoFormat) {
				// �t�H�[�}�b�g���ς����
				videoFormat = frame.format;
				onVideoFormatChanged(frame.format);
			}

			onVideoPesPacket(clock, frameInfo, packet);
		}
	}

	virtual void onVideoPesPacket(int64_t clock, const std::vector<VideoFrameInfo>& frames, PESPacket packet) = 0;

	virtual void onVideoFormatChanged(VideoFormat fmt) = 0;

private:
	VIDEO_STREAM_FORMAT videoStreamFormat;
	VideoFormat videoFormat;

	std::vector<VideoFrameInfo> frameInfo;

	MPEG2VideoParser mpeg2parser;
	H264VideoParser h264parser;

	IVideoParser* parser;

};

class AudioFrameParser : public AMTObject, public PesParser {
public:
	AudioFrameParser(AMTContext&ctx)
		: AMTObject(ctx)
		, PesParser()
		, adtsParser(ctx)
	{ }

	virtual void onPesPacket(int64_t clock, PESPacket packet) {
		if (clock == -1) {
			ctx.error("Audio PES Packet �ɃN���b�N��񂪂���܂���");
			return;
		}

		int64_t PTS = packet.has_PTS() ? packet.PTS : -1;
		int64_t DTS = packet.has_DTS() ? packet.DTS : PTS;
		MemoryChunk payload = packet.paylod();

		adtsParser.inputFrame(payload, frameData, PTS);

		if (frameData.size() > 0) {
			const AudioFrameData& frame = frameData[0];

			if (frame.format != format) {
				// �t�H�[�}�b�g���ς����
				format = frame.format;
				onAudioFormatChanged(frame.format);
			}

			onAudioPesPacket(clock, frameData, packet);
		}
	}

	virtual void onAudioPesPacket(int64_t clock, const std::vector<AudioFrameData>& frames, PESPacket packet) = 0;

	virtual void onAudioFormatChanged(AudioFormat fmt) = 0;

private:
	AudioFormat format;

	std::vector<AudioFrameData> frameData;

	AdtsParser adtsParser;
};

// �����^�̎����̂ݑΉ��B�����X�[�p�[�ɂ͑Ή����Ȃ�
class CaptionParser : public AMTObject, public PesParser {
public:
	CaptionParser(AMTContext&ctx)
		: AMTObject(ctx)
		, PesParser()
		, fomatter(*this)
	{ }

	virtual void onPesPacket(int64_t clock, PESPacket packet)
	{
		int64_t PTS = packet.has_PTS() ? packet.PTS : -1;
		int64_t SysClock = clock / 300;

		// ��{�I��PTS�������\�Ȏ�M�@�����A
		// PTS���������Ȃ��ꍇ������̂ŁA���̏ꍇ��
		// PTS�������s�\�Ȏ�M�@���G�~���[���[�V����
		// PTS��SysClock�Ƃ̍���
		//  - ARIB�ɂ��Ύ�M����\���܂ŏ��Ȃ��Ƃ�0.5�b�ȏ�󂯂�悤�ɂƂ���
		//  - ������TS���m�F�����Ƃ��낾������0.75�`0.80�b���炢������
		// �̂ŁA0.5�b�`1.5�b�𐳂����Ɣ���A����ȊO��0.8�b���\�������Ƃ���
		auto Td = PTS - SysClock;
		if (Td < 0.5 * MPEG_CLOCK_HZ || Td > 1.5 * MPEG_CLOCK_HZ) {
			PTS = SysClock + (int)(0.8 * MPEG_CLOCK_HZ);
			//ctx.info("����PTS���C�� %d", PTS);
		}

		//int64_t DTS = packet.has_DTS() ? packet.DTS : PTS;
		MemoryChunk payload = packet.paylod();

		captions.clear();

		DWORD ret = AddPESPacketCP(payload.data, (DWORD)payload.length);

		if (ret >= CP_NO_ERR_CAPTION_1 && ret <= CP_NO_ERR_CAPTION_8) {
			int ucLangTag = ret - CP_NO_ERR_CAPTION_1;
			// �������f�[�^
			CAPTION_DATA_DLL* capList = nullptr;
			DWORD capCount = 0;
			DRCS_PATTERN_DLL* drcsList = nullptr;
			DWORD drcsCount = 0;
			if (GetCaptionDataCPW(ucLangTag, &capList, &capCount) != TRUE) {
				capCount = 0;
			}
			else {
				// DRCS�}�`�f�[�^(�����)
				if (GetDRCSPatternCP(ucLangTag, &drcsList, &drcsCount) != TRUE) {
					drcsCount = 0;
				}
				for (DWORD i = 0; i < capCount; ++i) {
					captions.emplace_back(
						fomatter.ProcessCaption(PTS, ucLangTag,
							capList + i, capCount - i, drcsList, drcsCount));
				}
			}
		}
		else if (ret == CP_CHANGE_VERSION) {
			// �����Ǘ��f�[�^
			// �����̃t�H�[�}�b�g�ύX�͍l�����Ȃ��̂ō��̂Ƃ��댩��K�v�Ȃ�
		}
		else if (ret == CP_NO_ERR_TAG_INFO) {
			//
		}
		else if (ret != TRUE && ret != CP_ERR_NEED_NEXT_PACKET &&
			(ret < CP_NO_ERR_CAPTION_1 || CP_NO_ERR_CAPTION_8 < ret))
		{
			// �G���[�p�P�b�g
		}

		if (captions.size() > 0) {
			onCaptionPesPacket(clock, captions, packet);
		}
	}

	virtual void onCaptionPesPacket(int64_t clock, std::vector<CaptionItem>& captions, PESPacket packet) = 0;

	virtual DRCSOutInfo getDRCSOutPath(int64_t PTS, const std::string& md5) = 0;

private:
	class SpCaptionFormatter : public CaptionDLLParser {
		CaptionParser& this_;
	public:
		SpCaptionFormatter(CaptionParser& this_)
			: CaptionDLLParser(this_.ctx), this_(this_)
		{ }
		virtual DRCSOutInfo getDRCSOutPath(int64_t PTS, const std::string& md5) {
			return this_.getDRCSOutPath(PTS, md5);
		}
	};
	std::vector<CaptionItem> captions;
	SpCaptionFormatter fomatter;
};

// TS�X�g���[�������ʂ����߂��悤�ɂ���
class TsPacketBuffer : public TsPacketParser {
public:
	TsPacketBuffer(AMTContext& ctx)
		: TsPacketParser(ctx)
		, handler(NULL)
		, numBefferedPackets_(0)
		, numMaxPackets(0)
		, buffering(false)
	{ }

	void setHandler(TsPacketHandler* handler) {
		this->handler = handler;
	}

	int numBefferedPackets() {
		return numBefferedPackets_;
	}

	void clearBuffer() {
		buffer.clear();
		numBefferedPackets_ = 0;
	}

	void setEnableBuffering(bool enable) {
		buffering = enable;
		if (!buffering) {
			clearBuffer();
		}
	}

	void setNumBufferingPackets(int numPackets) {
		numMaxPackets = numPackets;
	}

	void backAndInput() {
		if (handler != NULL) {
			for (int i = 0; i < (int)buffer.size(); i += TS_PACKET_LENGTH) {
				TsPacket packet(buffer.ptr() + i);
				if (packet.parse() && packet.check()) {
					handler->onTsPacket(-1, packet);
				}
			}
		}
	}

	virtual void onTsPacket(TsPacket packet) {
		if (buffering) {
			if (numBefferedPackets_ >= numMaxPackets) {
				buffer.trimHead((numMaxPackets - numBefferedPackets_ + 1) * TS_PACKET_LENGTH);
				numBefferedPackets_ = numMaxPackets - 1;
			}
			buffer.add(MemoryChunk(packet.data, TS_PACKET_LENGTH));
			++numBefferedPackets_;
		}
		if (handler != NULL) {
			handler->onTsPacket(-1, packet);
		}
	}

private:
	TsPacketHandler* handler;
	AutoBuffer buffer;
	int numBefferedPackets_;
	int numMaxPackets;
	bool buffering;
};

class TsSystemClock {
public:
	TsSystemClock()
		: PcrPid(-1)
		, numPcrReceived(0)
		, numTotakPacketsReveived(0)
		, pcrInfo()
	{ }

	void setPcrPid(int PcrPid) {
		this->PcrPid = PcrPid;
	}

	// �\���Ȑ���PCR����M������
	bool pcrReceived() {
		return numPcrReceived >= 2;
	}

	// ���ݓ��͂��ꂽ�p�P�b�g����ɂ���relative������̃p�P�b�g�̓��͎�����Ԃ�
	int64_t getClock(int relative) {
		if (!pcrReceived()) {
			return -1;
		}
		int index = numTotakPacketsReveived + relative - 1;
		int64_t clockDiff = pcrInfo[1].clock - pcrInfo[0].clock;
		int64_t indexDiff = pcrInfo[1].packetIndex - pcrInfo[0].packetIndex;
		return clockDiff * (index - pcrInfo[1].packetIndex) / indexDiff + pcrInfo[1].clock;
	}

	// TS�X�g���[�����ŏ�����ǂݒ����Ƃ��ɌĂяo��
	void backTs() {
		numTotakPacketsReveived = 0;
	}

	// TS�X�g���[���̑S�f�[�^�����邱��
	void inputTsPacket(TsPacket packet) {
		if (packet.PID() == PcrPid) {
			if (packet.has_adaptation_field()) {
				MemoryChunk data = packet.adapdation_field();
				AdapdationField af(data.data, (int)data.length);
				if (af.parse() && af.check()) {
					if (af.discontinuity_indicator()) {
						// PCR���A���łȂ��̂Ń��Z�b�g
						numPcrReceived = 0;
					}
					if (pcrInfo[1].packetIndex < numTotakPacketsReveived) {
						std::swap(pcrInfo[0], pcrInfo[1]);
						if (af.PCR_flag()) {
							pcrInfo[1].clock = af.program_clock_reference;
							pcrInfo[1].packetIndex = numTotakPacketsReveived;
							++numPcrReceived;
						}

						// �e�X�g�p
						//if (pcrReceived()) {
						//	PRINTF("PCR: %f Mbps\n", currentBitrate() / (1024 * 1024));
						//}
					}
				}
			}
		}
		++numTotakPacketsReveived;
	}

	double currentBitrate() {
		int clockDiff = int(pcrInfo[1].clock - pcrInfo[0].clock);
		int indexDiff = int(pcrInfo[1].packetIndex - pcrInfo[0].packetIndex);
		return (double)(indexDiff * TS_PACKET_LENGTH * 8) / clockDiff * 27000000;
	}

private:
	struct PCR_Info {
		int64_t clock = 0;
		int packetIndex = -1;
	};

	int PcrPid;
	int numPcrReceived;
	int numTotakPacketsReveived;
	PCR_Info pcrInfo[2];
};

class TsSplitter : public AMTObject, protected TsPacketSelectorHandler {
public:
	TsSplitter(AMTContext& ctx, bool enableVideo, bool enableAudio, bool enableCaption)
		: AMTObject(ctx)
		, initPhase(PMT_WAITING)
		, tsPacketHandler(*this)
		, pcrDetectionHandler(*this)
		, tsPacketParser(ctx)
		, tsPacketSelector(ctx)
		, videoParser(ctx, *this)
		, captionParser(ctx, *this)
		, enableVideo(enableVideo)
		, enableAudio(enableAudio)
		, enableCaption(enableCaption)
		, numTotalPackets(0)
		, numScramblePackets(0)
	{
		tsPacketParser.setHandler(&tsPacketHandler);
		tsPacketParser.setNumBufferingPackets(50 * 1024); // 9.6MB
		tsPacketSelector.setHandler(this);
		reset();
	}

	void reset() {
		initPhase = PMT_WAITING;
		preferedServiceId = -1;
		selectedServiceId = -1;
		tsPacketParser.setEnableBuffering(true);
	}

	// 0�ȉ��Ŏw�薳��
	void setServiceId(int sid) {
		preferedServiceId = sid;
	}

	int getActualServiceId() const {
		return selectedServiceId;
	}

	void inputTsData(MemoryChunk data) {
		tsPacketParser.inputTS(data);
	}
	void flush() {
		tsPacketParser.flush();
	}

	int64_t getNumTotalPackets() const {
		return numTotalPackets;
	}

	int64_t getNumScramblePackets() const {
		return numScramblePackets;
	}

protected:
	enum INITIALIZATION_PHASE {
		PMT_WAITING,	// PAT,PMT�҂�
		PCR_WAITING,	// �r�b�g���[�g�擾�̂���PCR2����M��
		INIT_FINISHED,	// �K�v�ȏ��͑�����
	};

	class SpTsPacketHandler : public TsPacketHandler {
		TsSplitter& this_;
	public:
		SpTsPacketHandler(TsSplitter& this_)
			: this_(this_) { }

		virtual void onTsPacket(int64_t clock, TsPacket packet) {
			this_.tsSystemClock.inputTsPacket(packet);

			int64_t packetClock = this_.tsSystemClock.getClock(0);
			this_.tsPacketSelector.inputTsPacket(packetClock, packet);
		}
	};
	class PcrDetectionHandler : public TsPacketHandler {
		TsSplitter& this_;
	public:
		PcrDetectionHandler(TsSplitter& this_)
			: this_(this_) { }

		virtual void onTsPacket(int64_t clock, TsPacket packet) {
			this_.tsSystemClock.inputTsPacket(packet);
			if (this_.tsSystemClock.pcrReceived()) {
				this_.ctx.debug("�K�v�ȏ��͎擾�����̂�TS���ŏ�����ǂݒ����܂�");
				this_.initPhase = INIT_FINISHED;
				// �n���h����߂��čŏ�����ǂݒ���
				this_.tsPacketParser.setHandler(&this_.tsPacketHandler);
				this_.tsPacketSelector.resetParser();
				this_.tsSystemClock.backTs();

				int64_t startClock = this_.tsSystemClock.getClock(0);
				this_.ctx.infoF("�J�nClock: %lld", startClock);
				this_.tsPacketSelector.setStartClock(startClock);

				this_.tsPacketParser.backAndInput();
				// �����K�v�Ȃ��̂Ńo�b�t�@�����O��OFF
				this_.tsPacketParser.setEnableBuffering(false);
			}
		}
	};
	class SpVideoFrameParser : public VideoFrameParser {
		TsSplitter& this_;
	public:
		SpVideoFrameParser(AMTContext&ctx, TsSplitter& this_)
			: VideoFrameParser(ctx), this_(this_) { }

	protected:
		virtual void onVideoPesPacket(int64_t clock, const std::vector<VideoFrameInfo>& frames, PESPacket packet) {
			if (clock == -1) {
				ctx.error("Video PES Packet �ɃN���b�N��񂪂���܂���");
				return;
			}
			this_.onVideoPesPacket(clock, frames, packet);
		}

		virtual void onVideoFormatChanged(VideoFormat fmt) {
			this_.onVideoFormatChanged(fmt);
		}
	};
	class SpAudioFrameParser : public AudioFrameParser {
		TsSplitter& this_;
		int audioIdx;
	public:
		SpAudioFrameParser(AMTContext&ctx, TsSplitter& this_, int audioIdx)
			: AudioFrameParser(ctx), this_(this_), audioIdx(audioIdx) { }

	protected:
		virtual void onAudioPesPacket(int64_t clock, const std::vector<AudioFrameData>& frames, PESPacket packet) {
			this_.onAudioPesPacket(audioIdx, clock, frames, packet);
		}

		virtual void onAudioFormatChanged(AudioFormat fmt) {
			this_.onAudioFormatChanged(audioIdx, fmt);
		}
	};
	class SpCaptionParser : public CaptionParser {
		TsSplitter& this_;
	public:
		SpCaptionParser(AMTContext&ctx, TsSplitter& this_)
			: CaptionParser(ctx), this_(this_) { }

	protected:
		virtual void onCaptionPesPacket(int64_t clock, std::vector<CaptionItem>& captions, PESPacket packet) {
			this_.onCaptionPesPacket(clock, captions, packet);
		}

		virtual DRCSOutInfo getDRCSOutPath(int64_t PTS, const std::string& md5) {
			return this_.getDRCSOutPath(PTS, md5);
		}
	};

	INITIALIZATION_PHASE initPhase;

	TsPacketBuffer tsPacketParser;
	TsSystemClock tsSystemClock;
	SpTsPacketHandler tsPacketHandler;
	PcrDetectionHandler pcrDetectionHandler;
	TsPacketSelector tsPacketSelector;

	SpVideoFrameParser videoParser;
	std::vector<SpAudioFrameParser*> audioParsers;
	SpCaptionParser captionParser;

	bool enableVideo;
	bool enableAudio;
	bool enableCaption;
	int preferedServiceId;
	int selectedServiceId;

	int64_t numTotalPackets;
	int64_t numScramblePackets;

	virtual void onVideoPesPacket(
		int64_t clock,
		const std::vector<VideoFrameInfo>& frames,
		PESPacket packet) = 0;

	virtual void onVideoFormatChanged(VideoFormat fmt) = 0;

	virtual void onAudioPesPacket(
		int audioIdx,
		int64_t clock,
		const std::vector<AudioFrameData>& frames,
		PESPacket packet) = 0;

	virtual void onAudioFormatChanged(int audioIdx, AudioFormat fmt) = 0;

	virtual void onCaptionPesPacket(
		int64_t clock,
		std::vector<CaptionItem>& captions,
		PESPacket packet) = 0;

	virtual DRCSOutInfo getDRCSOutPath(int64_t PTS, const std::string& md5) = 0;

	// �T�[�r�X��ݒ肷��ꍇ�̓T�[�r�X��pids��ł̃C���f�b�N�X
	// �Ȃɂ����Ȃ��ꍇ�͕��̒l�̕Ԃ�
	virtual int onPidSelect(int TSID, const std::vector<int>& pids) {
		ctx.info("[PAT�X�V]");
		for (int i = 0; i < int(pids.size()); ++i) {
			if (preferedServiceId == pids[i]) {
				selectedServiceId = pids[i];
				ctx.infoF("�T�[�r�X %d ��I��", selectedServiceId);
				return i;
			}
		}
		if (preferedServiceId > 0) {
			// �T�[�r�X�w�肪����̂ɊY���T�[�r�X���Ȃ�������G���[�Ƃ���
			StringBuilder sb;
			sb.append("�T�[�r�XID: ");
			for (int i = 0; i < (int)pids.size(); ++i) {
				sb.append("%s%d", (i > 0) ? ", " : "", pids[i]);
			}
			sb.append(" �w��T�[�r�XID: %d", preferedServiceId);
			ctx.error("�w�肳�ꂽ�T�[�r�X������܂���");
			ctx.error(sb.str().c_str());
			//THROW(InvalidOperationException, "failed to select service");
		}
		selectedServiceId = pids[0];
		ctx.infoF("�T�[�r�X %d ��I���i�w�肪����܂���ł����j", selectedServiceId);
		return 0;
	}

	virtual void onPmtUpdated(int PcrPid) {
		if (initPhase == PMT_WAITING) {
			initPhase = PCR_WAITING;
			// PCR�n���h���ɒu��������TS���ŏ�����ǂݒ���
			tsPacketParser.setHandler(&pcrDetectionHandler);
			tsSystemClock.setPcrPid(PcrPid);
			tsPacketSelector.resetParser();
			tsSystemClock.backTs();
			tsPacketParser.backAndInput();
		}
	}

	// TsPacketSelector��PID Table���ύX���ꂽ���ύX��̏�񂪑�����
	virtual void onPidTableChanged(const PMTESInfo video, const std::vector<PMTESInfo>& audio, const PMTESInfo caption) {
		if (enableVideo || enableAudio) {
			// �f���X�g���[���`�����Z�b�g
			switch (video.stype) {
			case 0x02: // MPEG2-VIDEO
				videoParser.setStreamFormat(VS_MPEG2);
				break;
			case 0x1B: // H.264/AVC
				videoParser.setStreamFormat(VS_H264);
				break;
			}

			// �K�v�Ȑ����������p�[�T���
			size_t numAudios = audio.size();
			while (audioParsers.size() < numAudios) {
				int audioIdx = int(audioParsers.size());
				audioParsers.push_back(new SpAudioFrameParser(ctx, *this, audioIdx));
				ctx.infoF("�����p�[�T %d ��ǉ�", audioIdx);
			}
		}
	}

	bool checkScramble(TsPacket packet) {
		++numTotalPackets;
		if (packet.transport_scrambling_control()) {
			++numScramblePackets;
			return false;
		}
		return true;
	}

	virtual void onVideoPacket(int64_t clock, TsPacket packet) {
		if (enableVideo && checkScramble(packet)) videoParser.onTsPacket(clock, packet);
	}

	virtual void onAudioPacket(int64_t clock, TsPacket packet, int audioIdx) {
		if (enableAudio && checkScramble(packet)) {
			ASSERT(audioIdx < (int)audioParsers.size());
			audioParsers[audioIdx]->onTsPacket(clock, packet);
		}
	}

	virtual void onCaptionPacket(int64_t clock, TsPacket packet) {
		if (enableCaption && checkScramble(packet)) captionParser.onTsPacket(clock, packet);
	}
};

