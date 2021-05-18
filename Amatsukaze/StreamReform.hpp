/**
* Output stream construction
* Copyright (c) 2017-2019 Nekopanda
*
* This software is released under the MIT License.
* http://opensource.org/licenses/mit-license.php
*/
#pragma once

#include <time.h>

#include <vector>
#include <map>
#include <memory>
#include <functional>

#include "StreamUtils.hpp"

// ���Ԃ͑S�� 90kHz double �Ōv�Z����
// 90kHz�ł�60*1000/1001fps��1�t���[���̎��Ԃ͐����ŕ\���Ȃ�
// ������ƌ�����27MHz�ł͐��l���傫������

struct FileAudioFrameInfo : public AudioFrameInfo {
	int audioIdx;
	int codedDataSize;
	int waveDataSize;
	int64_t fileOffset;
	int64_t waveOffset;

	FileAudioFrameInfo()
		: AudioFrameInfo()
		, audioIdx(0)
		, codedDataSize(0)
		, waveDataSize(0)
		, fileOffset(0)
		, waveOffset(-1)
	{ }

	FileAudioFrameInfo(const AudioFrameInfo& info)
		: AudioFrameInfo(info)
		, audioIdx(0)
		, codedDataSize(0)
		, waveDataSize(0)
		, fileOffset(0)
		, waveOffset(-1)
	{ }
};

struct FileVideoFrameInfo : public VideoFrameInfo {
	int64_t fileOffset;

	FileVideoFrameInfo()
		: VideoFrameInfo()
		, fileOffset(0)
	{ }

	FileVideoFrameInfo(const VideoFrameInfo& info)
		: VideoFrameInfo(info)
		, fileOffset(0)
	{ }
};

enum StreamEventType {
	STREAM_EVENT_NONE = 0,
	PID_TABLE_CHANGED,
	VIDEO_FORMAT_CHANGED,
	AUDIO_FORMAT_CHANGED
};

struct StreamEvent {
	StreamEventType type;
	int frameIdx;	// �t���[���ԍ�
	int audioIdx;	// �ύX���ꂽ�����C���f�b�N�X�iAUDIO_FORMAT_CHANGED�̂Ƃ��̂ݗL���j
	int numAudio;	// �����̐��iPID_TABLE_CHANGED�̂Ƃ��̂ݗL���j
};

typedef std::vector<std::vector<int>> FileAudioFrameList;

struct OutVideoFormat {
	int formatId; // �����t�H�[�}�b�gID�i�ʂ��ԍ��j
	int videoFileId;
	VideoFormat videoFormat;
	std::vector<AudioFormat> audioFormat;
};

// ���Y�����v���
struct AudioDiffInfo {
	double sumPtsDiff;
	int totalSrcFrames;
	int totalAudioFrames; // �o�͂��������t���[���i�����������܂ށj
	int totalUniquAudioFrames; // �o�͂��������t���[���i�����������܂܂��j
	double maxPtsDiff;
	double maxPtsDiffPos;
	double basePts;

	// �b�P�ʂŎ擾
	double avgDiff() const {
		return ((double)sumPtsDiff / totalAudioFrames) / MPEG_CLOCK_HZ;
	}
	// �b�P�ʂŎ擾
	double maxDiff() const {
		return (double)maxPtsDiff / MPEG_CLOCK_HZ;
	}

	void printAudioPtsDiff(AMTContext& ctx) const {
		double avgDiff = this->avgDiff() * 1000;
		double maxDiff = this->maxDiff() * 1000;
		int notIncluded = totalSrcFrames - totalUniquAudioFrames;

		ctx.infoF("�o�͉����t���[��: %d�i�����������t���[��%d�j",
			totalAudioFrames, totalAudioFrames - totalUniquAudioFrames);
		ctx.infoF("���o�̓t���[��: %d�i%.3f%%�j",
			notIncluded, (double)notIncluded * 100 / totalSrcFrames);

		ctx.infoF("���Y��: ���� %.2fms �ő� %.2fms",
			avgDiff, maxDiff);
		if (maxPtsDiff > 0 && maxDiff - avgDiff > 1) {
			double sec = elapsedTime(maxPtsDiffPos);
			int minutes = (int)(sec / 60);
			sec -= minutes * 60;
			ctx.infoF("�ő剹�Y���ʒu: ���͍ŏ��̉f���t���[������%d��%.3f�b��",
				minutes, sec);
		}
	}

	void printToJson(StringBuilder& sb) {
		double avgDiff = this->avgDiff() * 1000;
		double maxDiff = this->maxDiff() * 1000;
		int notIncluded = totalSrcFrames - totalUniquAudioFrames;
		double maxDiffPos = maxPtsDiff > 0 ? elapsedTime(maxPtsDiffPos) : 0.0;

		sb.append(
			"{ \"totalsrcframes\": %d, \"totaloutframes\": %d, \"totaloutuniqueframes\": %d, "
			"\"notincludedper\": %g, \"avgdiff\": %g, \"maxdiff\": %g, \"maxdiffpos\": %g }",
			totalSrcFrames, totalAudioFrames, totalUniquAudioFrames,
			(double)notIncluded * 100 / totalSrcFrames, avgDiff, maxDiff, maxDiffPos);
	}

private:
	double elapsedTime(double modPTS) const {
		return (double)(modPTS - basePts) / MPEG_CLOCK_HZ;
	}
};

struct FilterSourceFrame {
	bool halfDelay;
	int frameIndex; // �����p(DTS���t���[���ԍ�)
	double pts; // �����p
	double frameDuration; // �����p
	int64_t framePTS;
	int64_t fileOffset;
	int keyFrame;
	CMType cmType;
};

struct FilterAudioFrame {
	int frameIndex; // �f�o�b�O�p
	int64_t waveOffset;
	int waveLength;
};

struct FilterOutVideoInfo {
	int numFrames;
	int frameRateNum;
	int frameRateDenom;
	int fakeAudioSampleRate;
	std::vector<int> fakeAudioSamples;
};

struct OutCaptionLine {
	double start, end;
	CaptionLine* line;
};

typedef std::vector<std::vector<OutCaptionLine>> OutCaptionList;

struct NicoJKLine {
	double start, end;
	std::string line;

	void Write(const File& file) const {
		file.writeValue(start);
		file.writeValue(end);
		file.writeString(line);
	}

	static NicoJKLine Read(const File& file) {
		NicoJKLine item;
		item.start = file.readValue<double>();
		item.end = file.readValue<double>();
		item.line = file.readString();
		return item;
	}
};

typedef std::array<std::vector<NicoJKLine>, NICOJK_MAX> NicoJKList;

typedef std::pair<int64_t, JSTTime> TimeInfo;

struct EncodeFileInput {
	EncodeFileKey key;     // �L�[
	EncodeFileKey outKey; // �o�̓t�@�C�����p�L�[
	EncodeFileKey keyMax;  // �o�̓t�@�C��������p�ő�l
	double duration;       // �Đ�����
	std::vector<int> videoFrames; // �f���t���[�����X�g�i���g�̓t�B���^���̓t���[���ł̃C���f�b�N�X�j
	FileAudioFrameList audioFrames; // �����t���[�����X�g
	OutCaptionList captionList;     // ����
	NicoJKList nicojkList;          // �j�R�j�R�����R�����g
};

class StreamReformInfo : public AMTObject {
public:
	StreamReformInfo(
		AMTContext& ctx,
		int numVideoFile,
		std::vector<FileVideoFrameInfo>& videoFrameList,
		std::vector<FileAudioFrameInfo>& audioFrameList,
		std::vector<CaptionItem>& captionList,
		std::vector<StreamEvent>& streamEventList,
		std::vector<TimeInfo>& timeList)
		: AMTObject(ctx)
		, numVideoFile_(numVideoFile)
		, videoFrameList_(std::move(videoFrameList))
		, audioFrameList_(std::move(audioFrameList))
		, captionItemList_(std::move(captionList))
		, streamEventList_(std::move(streamEventList))
		, timeList_(std::move(timeList))
		, isVFR_(false)
		, hasRFF_(false)
		, srcTotalDuration_()
		, outTotalDuration_()
		, firstFrameTime_()
	{ }

	// 1. �R���X�g���N�g����ɌĂ�
	// splitSub: ���C���ȊO�̃t�H�[�}�b�g���������Ȃ�
	void prepare(bool splitSub, bool isEncodeAudio) {
		isEncodeAudio_ = isEncodeAudio;
		reformMain(splitSub);
		genWaveAudioStream();
	}

	time_t getFirstFrameTime() const {
		return firstFrameTime_;
	}

	// 2. �j�R�j�R�����R�����g���擾������Ă�
	void SetNicoJKList(const std::array<std::vector<NicoJKLine>, NICOJK_MAX>& nicoJKList) {
		for (int t = 0; t < NICOJK_MAX; ++t) {
			nicoJKList_[t].resize(nicoJKList[t].size());
			double startTime = dataPTS_.front();
			for (int i = 0; i < (int)nicoJKList[t].size(); ++i) {
				auto& src = nicoJKList[t][i];
				auto& dst = nicoJKList_[t][i];
				// �J�n�f���I�t�Z�b�g�����Z
				dst.start = src.start + startTime;
				dst.end = src.end + startTime;
				dst.line = src.line;
			}
		}
	}

	// 2. �e���ԉf���t�@�C����CM��͌�ɌĂ�
	// cmzones: CM�]�[���i�t�B���^���̓t���[���ԍ��j
	// divs: �����|�C���g���X�g�i�t�B���^���̓t���[���ԍ��j
	void applyCMZones(int videoFileIndex, const std::vector<EncoderZone>& cmzones, const std::vector<int>& divs) {
		auto& frames = filterFrameList_[videoFileIndex];
		for (auto zone : cmzones) {
			for (int i = zone.startFrame; i < zone.endFrame; ++i) {
				frames[i].cmType = CMTYPE_CM;
			}
		}
		fileDivs_[videoFileIndex] = divs;
	}

	// 3. CM��͂��I��������G���R�[�h�O�ɌĂ�
	// cmtypes: �o�͂���CM�^�C�v���X�g
	AudioDiffInfo genAudio(const std::vector<CMType>& cmtypes) {
		calcSizeAndTime(cmtypes);
		genCaptionStream();
		return genAudioStream();
	}

	// ���ԉf���t�@�C���̌�
	int getNumVideoFile() const {
		return numVideoFile_;
	}

	// ���͉f���K�i
	VIDEO_STREAM_FORMAT getVideoStreamFormat() const {
		return videoFrameList_[0].format.format;
	}

	// PMT�ύXPTS���X�g
	std::vector<int> getPidChangedList(int videoFileIndex) const {
		std::vector<int> ret;
		auto& frames = filterFrameList_[videoFileIndex];
		for (int i = 0; i < (int)streamEventList_.size(); ++i) {
			if (streamEventList_[i].type == PID_TABLE_CHANGED) {
				FilterSourceFrame tmp = FilterSourceFrame();
				tmp.pts = streamEventPTS_[i];
				auto idx = std::lower_bound(frames.begin(), frames.end(), tmp,
					[&](const FilterSourceFrame& e, const FilterSourceFrame& value) {
					return dataPTS_[e.frameIndex] < value.pts;
				}) - frames.begin();
				if (ret.size() == 0 || ret.back() != idx) {
					ret.push_back((int)idx);
				}
			}
		}
		return ret;
	}

	int getMainVideoFileIndex() const {
		int maxFrames = 0, maxIndex = 0;
		for (int i = 0; i < (int)filterFrameList_.size(); ++i) {
			if (maxFrames < filterFrameList_[i].size()) {
				maxFrames = (int)filterFrameList_[i].size();
				maxIndex = i;
			}
		}
		return maxIndex;
	}

	// �t�B���^���͉f���t���[��
	const std::vector<FilterSourceFrame>& getFilterSourceFrames(int videoFileIndex) const {
		return filterFrameList_[videoFileIndex];
	}

	// �t�B���^���͉����t���[��
	const std::vector<FilterAudioFrame>& getFilterSourceAudioFrames(int videoFileIndex) const {
		return filterAudioFrameList_[videoFileIndex];
	}

	// �o�̓t�@�C�����
	const EncodeFileInput& getEncodeFile(EncodeFileKey key) const {
		return outFiles_.at(key.key());
	}

	// ���Ԉꎞ�t�@�C�����Ƃ̏o�̓t�@�C����
	int getNumEncoders(int videoFileIndex) const {
		return int(
			fileFormatStartIndex_[videoFileIndex + 1] - fileFormatStartIndex_[videoFileIndex]);
	}

	// ���v�o�̓t�@�C����
	//int getNumOutFiles() const {
	//	return (int)fileFormatId_.size();
	//}

	// video frame index -> VideoFrameInfo
	const VideoFrameInfo& getVideoFrameInfo(int frameIndex) const {
		return videoFrameList_[frameIndex];
	}

	// video frame index (DTS��) -> encoder index
	int getEncoderIndex(int frameIndex) const {
		int fileId = frameFormatId_[frameIndex];
		const auto& format = format_[fileFormatId_[fileId]];
		return fileId - formatStartIndex_[format.videoFileId];
	}

	// key��video,format��2�����g���Ȃ�
	const OutVideoFormat& getFormat(EncodeFileKey key) const {
		int fileId = fileFormatStartIndex_[key.video] + key.format;
		return format_[fileFormatId_[fileId]];
	}

	// genAudio��g�p�\
	const std::vector<EncodeFileKey>& getOutFileKeys() const {
		return outFileKeys_;
	}

	// �f���f�[�^�T�C�Y�i�o�C�g�j�A���ԁi�^�C���X�^���v�j�̃y�A
	std::pair<int64_t, double> getSrcVideoInfo(int videoFileIndex) const {
		return std::make_pair(filterSrcSize_[videoFileIndex], filterSrcDuration_[videoFileIndex]);
	}

	// TODO: VFR�p�^�C���R�[�h�擾
	// infps: �t�B���^���͂�FPS
	// outpfs: �t�B���^�o�͂�FPS
	void getTimeCode(
		int encoderIndex, int videoFileIndex, CMType cmtype, double infps, double outfps) const
	{
		//
	}

	const std::vector<int64_t>& getAudioFileOffsets() const {
		return audioFileOffsets_;
	}

	bool isVFR() const {
		return isVFR_;
	}

	bool hasRFF() const {
		return hasRFF_;
	}

	double getInDuration() const {
		return srcTotalDuration_;
	}

	std::pair<double, double> getInOutDuration() const {
		return std::make_pair(srcTotalDuration_, outTotalDuration_);
	}

	// �����t���[���ԍ����X�g����FilterAudioFrame���X�g�ɕϊ�
	std::vector<FilterAudioFrame> getWaveInput(const std::vector<int>& frameList) const {
		std::vector<FilterAudioFrame> ret;
		for (int i = 0; i < (int)frameList.size(); ++i) {
			FilterAudioFrame frame = { 0 };
			auto& info = audioFrameList_[frameList[i]];
			frame.frameIndex = frameList[i];
			frame.waveOffset = info.waveOffset;
			frame.waveLength = info.waveDataSize;
			ret.push_back(frame);
		}
		return ret;
	}

	void printOutputMapping(std::function<tstring(EncodeFileKey)> getFileName) const
	{
		ctx.info("[�o�̓t�@�C��]");
		for (int i = 0; i < (int)outFileKeys_.size(); ++i) {
			ctx.infoF("%d: %s", i, getFileName(outFileKeys_[i]));
		}

		ctx.info("[����->�o�̓}�b�s���O]");
		double fromPTS = dataPTS_[0];
		int prevFileId = 0;
		for (int i = 0; i < (int)ordredVideoFrame_.size(); ++i) {
			int ordered = ordredVideoFrame_[i];
			double pts = modifiedPTS_[ordered];
			int fileId = frameFormatId_[ordered];
			if (prevFileId != fileId) {
				// print
				auto from = elapsedTime(fromPTS);
				auto to = elapsedTime(pts);
				ctx.infoF("%3d��%05.3f�b - %3d��%05.3f�b -> %d",
					from.first, from.second, to.first, to.second, fileFormatId_[prevFileId]);
				prevFileId = fileId;
				fromPTS = pts;
			}
		}
		auto from = elapsedTime(fromPTS);
		auto to = elapsedTime(dataPTS_.back());
		ctx.infoF("%3d��%05.3f�b - %3d��%05.3f�b -> %d",
			from.first, from.second, to.first, to.second, fileFormatId_[prevFileId]);
	}

	// �ȉ��f�o�b�O�p //

	void serialize(const tstring& path) {
		serialize(File(path, _T("wb")));
	}

	void serialize(const File& file) {
		file.writeValue(numVideoFile_);
		file.writeArray(videoFrameList_);
		file.writeArray(audioFrameList_);
		WriteArray(file, captionItemList_);
		file.writeArray(streamEventList_);
		file.writeArray(timeList_);

		file.writeValue(firstFrameTime_);
		file.writeValue(srcTotalDuration_);
	}

	static StreamReformInfo deserialize(AMTContext& ctx, const tstring& path) {
		return deserialize(ctx, File(path, _T("rb")));
	}

	static StreamReformInfo deserialize(AMTContext& ctx, const File& file) {
		int numVideoFile = file.readValue<int>();
		auto videoFrameList = file.readArray<FileVideoFrameInfo>();
		auto audioFrameList = file.readArray<FileAudioFrameInfo>();
		auto captionList = ReadArray<CaptionItem>(file);
		auto streamEventList = file.readArray<StreamEvent>();
		auto timeList = file.readArray<TimeInfo>();
		return StreamReformInfo(ctx,
			numVideoFile, videoFrameList, audioFrameList, captionList, streamEventList, timeList);
	}

private:

	struct CaptionDuration {
		double startPTS, endPTS;
	};

	// ��v�C���f�b�N�X�̐���
	// DTS��: �S�f���t���[����DTS���ŕ��ׂ��Ƃ��̃C���f�b�N�X
	// PTS��: �S�f���t���[����PTS���ŕ��ׂ��Ƃ��̃C���f�b�N�X
	// ���ԉf���t�@�C����: ���ԉf���t�@�C���̃C���f�b�N�X(=video)
	// �t�H�[�}�b�g��: �S�t�H�[�}�b�g�̃C���f�b�N�X
	// �t�H�[�}�b�g(�o��)��: ��{�I�Ƀt�H�[�}�b�g�Ɠ��������A�u���C���ȊO�͌������Ȃ��v�ꍇ�A
	//                     ���C���ȊO����������ĈقȂ�C���f�b�N�X�ɂȂ��Ă���(=format)
	// �o�̓t�@�C����: EncodeFileKey�Ŏ��ʂ����o�̓t�@�C���̃C���f�b�N�X

	// ���͉�͂̏o��
	int numVideoFile_;
	std::vector<FileVideoFrameInfo> videoFrameList_; // [DTS��] 
	std::vector<FileAudioFrameInfo> audioFrameList_;
	std::vector<CaptionItem> captionItemList_;
	std::vector<StreamEvent> streamEventList_;
	std::vector<TimeInfo> timeList_;

	std::array<std::vector<NicoJKLine>, NICOJK_MAX> nicoJKList_;
	bool isEncodeAudio_;

	// �v�Z�f�[�^
	bool isVFR_;
	bool hasRFF_;
	std::vector<double> modifiedPTS_; // [DTS��] ���b�v�A���E���h���Ȃ�PTS
	std::vector<double> modifiedAudioPTS_; // ���b�v�A���E���h���Ȃ�PTS
	std::vector<double> modifiedCaptionPTS_; // ���b�v�A���E���h���Ȃ�PTS
	std::vector<double> audioFrameDuration_; // �e�����t���[���̎���
	std::vector<int> ordredVideoFrame_; // [PTS��] -> [DTS��] �ϊ�
	std::vector<double> dataPTS_; // [DTS��] �f���t���[���̃X�g���[����ł̈ʒu��PTS�̊֘A�t��
	std::vector<double> streamEventPTS_;
	std::vector<CaptionDuration> captionDuration_;

	std::vector<std::vector<int>> indexAudioFrameList_; // �����C���f�b�N�X���Ƃ̃t���[�����X�g

	std::vector<OutVideoFormat> format_; // [�t�H�[�}�b�g��]
	// ���ԉf���t�@�C�����Ƃ̃t�H�[�}�b�g�J�n�C���f�b�N�X
	// �T�C�Y�͒��ԉf���t�@�C����+1
	std::vector<int> formatStartIndex_; // [���ԉf���t�@�C����]

	std::vector<int> fileFormatId_; // [�t�H�[�}�b�g(�o��)��] -> [�t�H�[�}�b�g��] �ϊ�
	// ���ԉf���t�@�C�����Ƃ̃t�@�C���J�n�C���f�b�N�X
	// �T�C�Y�͒��ԉf���t�@�C����+1
	std::vector<int> fileFormatStartIndex_; // [���ԉf���t�@�C����] -> [�t�H�[�}�b�g(�o��)��]

	// ���ԉf���t�@�C������
	std::vector<std::vector<FilterSourceFrame>> filterFrameList_; // [PTS��]
	std::vector<std::vector<FilterAudioFrame>> filterAudioFrameList_;
	std::vector<int64_t> filterSrcSize_;
	std::vector<double> filterSrcDuration_;
	std::vector<std::vector<int>> fileDivs_; // CM��͌���

	std::vector<int> frameFormatId_; // [DTS��] -> [�t�H�[�}�b�g(�o��)��]

	// �o�̓t�@�C�����X�g
	std::vector<EncodeFileKey> outFileKeys_; // [�o�̓t�@�C����]
	std::map<int, EncodeFileInput> outFiles_; // �L�[��EncodeFileKey.key()

	// �ŏ��̉f���t���[���̎���(UNIX����)
	time_t firstFrameTime_;

	std::vector<int64_t> audioFileOffsets_; // �����t�@�C���L���b�V���p

	double srcTotalDuration_;
	double outTotalDuration_;

	void reformMain(bool splitSub)
	{
		if (videoFrameList_.size() == 0) {
			THROW(FormatException, "�f���t���[����1��������܂���");
		}
		if (audioFrameList_.size() == 0) {
			THROW(FormatException, "�����t���[����1��������܂���");
		}
		if (streamEventList_.size() == 0 || streamEventList_[0].type != PID_TABLE_CHANGED) {
			THROW(FormatException, "�s���ȃf�[�^�ł�");
		}

		/*
		// framePtsMap_���쐬�i�����ɍ���̂Łj
		for (int i = 0; i < int(videoFrameList_.size()); ++i) {
			framePtsMap_[videoFrameList_[i].PTS] = i;
		}
		*/

		// VFR���o
		isVFR_ = false;
		for (int i = 0; i < int(videoFrameList_.size()); ++i) {
			if (videoFrameList_[i].format.fixedFrameRate == false) {
				isVFR_ = true;
				break;
			}
		}

		if (isVFR_) {
			THROW(FormatException, "���̃o�[�W������VFR�ɑΉ����Ă��܂���");
		}

		// �e�R���|�[�l���g�J�nPTS���f���t���[����̃��b�v�A���E���h���Ȃ�PTS�ɕϊ�
		//�i��������Ȃ��ƊJ�n�t���[�����m���ԂɃ��b�v�A���E���h������ł�Ɣ�r�ł��Ȃ��Ȃ�j
		std::vector<int64_t> startPTSs;
		startPTSs.push_back(videoFrameList_[0].PTS);
		startPTSs.push_back(audioFrameList_[0].PTS);
		if (captionItemList_.size() > 0) {
			startPTSs.push_back(captionItemList_[0].PTS);
		}
		int64_t modifiedStartPTS[3];
		int64_t prevPTS = startPTSs[0];
		for (int i = 0; i < int(startPTSs.size()); ++i) {
			int64_t PTS = startPTSs[i];
			int64_t modPTS = prevPTS + int64_t((int32_t(PTS) - int32_t(prevPTS)));
			modifiedStartPTS[i] = modPTS;
			prevPTS = modPTS;
		}

		// �e�R���|�[�l���g�̃��b�v�A���E���h���Ȃ�PTS�𐶐�
		makeModifiedPTS(modifiedStartPTS[0], modifiedPTS_, videoFrameList_);
		makeModifiedPTS(modifiedStartPTS[1], modifiedAudioPTS_, audioFrameList_);
		makeModifiedPTS(modifiedStartPTS[2], modifiedCaptionPTS_, captionItemList_);

		// audioFrameDuration_�𐶐�
		audioFrameDuration_.resize(audioFrameList_.size());
		for (int i = 0; i < (int)audioFrameList_.size(); ++i) {
			const auto& frame = audioFrameList_[i];
			audioFrameDuration_[i] = (frame.numSamples * MPEG_CLOCK_HZ) / (double)frame.format.sampleRate;
		}

		// ptsOrdredVideoFrame_�𐶐�
		ordredVideoFrame_.resize(videoFrameList_.size());
		for (int i = 0; i < (int)videoFrameList_.size(); ++i) {
			ordredVideoFrame_[i] = i;
		}
		std::sort(ordredVideoFrame_.begin(), ordredVideoFrame_.end(), [&](int a, int b) {
			return modifiedPTS_[a] < modifiedPTS_[b];
		});

		// dataPTS�𐶐�
		// ��납�猩�Ă��̎��_�ōł�������PTS��dataPTS�Ƃ���
		double curMin = INFINITY;
		double curMax = 0;
		dataPTS_.resize(videoFrameList_.size());
		for (int i = (int)videoFrameList_.size() - 1; i >= 0; --i) {
			curMin = std::min(curMin, modifiedPTS_[i]);
			curMax = std::max(curMax, modifiedPTS_[i]);
			dataPTS_[i] = curMin;
		}

		// �����̊J�n�E�I�����v�Z
		captionDuration_.resize(captionItemList_.size());
		double curEnd = dataPTS_.back();
		for (int i = (int)captionItemList_.size() - 1; i >= 0; --i) {
			double modPTS = modifiedCaptionPTS_[i] + (captionItemList_[i].waitTime * (MPEG_CLOCK_HZ / 1000));
			if (captionItemList_[i].line) {
				captionDuration_[i].startPTS = modPTS;
				captionDuration_[i].endPTS = curEnd;
			}
			else {
				// �N���A
				captionDuration_[i].startPTS = captionDuration_[i].endPTS = modPTS;
				// �I�����X�V
				curEnd = modPTS;
			}
		}

		// �X�g���[���C�x���g��PTS���v�Z
		double endPTS = curMax + 1;
		streamEventPTS_.resize(streamEventList_.size());
		for (int i = 0; i < (int)streamEventList_.size(); ++i) {
			auto& ev = streamEventList_[i];
			double pts = -1;
			if (ev.type == PID_TABLE_CHANGED || ev.type == VIDEO_FORMAT_CHANGED) {
				if (ev.frameIdx >= (int)videoFrameList_.size()) {
					// ���߂��đΏۂ̃t���[�����Ȃ�
					pts = endPTS;
				}
				else {
					pts = dataPTS_[ev.frameIdx];
				}
			}
			else if (ev.type == AUDIO_FORMAT_CHANGED) {
				if (ev.frameIdx >= (int)audioFrameList_.size()) {
					// ���߂��đΏۂ̃t���[�����Ȃ�
					pts = endPTS;
				}
				else {
					pts = modifiedAudioPTS_[ev.frameIdx];
				}
			}
			streamEventPTS_[i] = pts;
		}

		// ���ԓI�ɋ߂��X�g���[���C�x���g��1�̕ω��_�Ƃ݂Ȃ�
		const double CHANGE_TORELANCE = 3 * MPEG_CLOCK_HZ;

		std::vector<int> sectionFormatList;
		std::vector<double> startPtsList;

		ctx.info("[�t�H�[�}�b�g�؂�ւ����]");

		// ���݂̉����t�H�[�}�b�g��ێ�
		// ����ES�����ω����Ă��O�̉����t�H�[�}�b�g�ƕς��Ȃ��ꍇ��
		// �C�x���g�����ł��Ȃ��̂ŁA���݂̉���ES���Ƃ͊֌W�Ȃ��S�����t�H�[�}�b�g��ێ�����
		std::vector<AudioFormat> curAudioFormats;

		OutVideoFormat curFormat = OutVideoFormat();
		double startPts = -1;
		double curFromPTS = -1;
		double curVideoFromPTS = -1;
		curFormat.videoFileId = -1;
		auto addSection = [&]() {
			registerOrGetFormat(curFormat);
			sectionFormatList.push_back(curFormat.formatId);
			startPtsList.push_back(curFromPTS);
			if (startPts == -1) {
				startPts = curFromPTS;
			}
			ctx.infoF("%.2f -> %d", (curFromPTS - startPts) / 90000.0, curFormat.formatId);
			curFromPTS = -1;
			curVideoFromPTS = -1;
		};
		for (int i = 0; i < (int)streamEventList_.size(); ++i) {
			auto& ev = streamEventList_[i];
			double pts = streamEventPTS_[i];
			if (pts >= endPTS) {
				// ���ɉf�����Ȃ���ΈӖ����Ȃ�
				continue;
			}
			if (curFromPTS != -1 &&          // from������
				curFormat.videoFileId >= 0 &&  // �f��������
				curFromPTS + CHANGE_TORELANCE < pts) // CHANGE_TORELANCE��藣��Ă���
			{
				// ��Ԃ�ǉ�
				addSection();
			}
			// �ύX�𔽉f
			switch (ev.type) {
			case PID_TABLE_CHANGED:
				if (curAudioFormats.size() < ev.numAudio) {
					curAudioFormats.resize(ev.numAudio);
				}
				if (curFormat.audioFormat.size() != ev.numAudio) {
					curFormat.audioFormat.resize(ev.numAudio);
					for (int i = 0; i < ev.numAudio; ++i) {
						curFormat.audioFormat[i] = curAudioFormats[i];
					}
					if (curFromPTS == -1) {
						curFromPTS = pts;
					}
				}
				break;
			case VIDEO_FORMAT_CHANGED:
				// �t�@�C���ύX
				if (!curFormat.videoFormat.isBasicEquals(videoFrameList_[ev.frameIdx].format)) {
					// �A�X�y�N�g��ȊO���ύX����Ă�����t�@�C���𕪂���
					//�iAMTSplitter�Ə��������킹�Ȃ���΂Ȃ�Ȃ����Ƃɒ��Ӂj
					++curFormat.videoFileId;
					formatStartIndex_.push_back((int)format_.size());
				}
				curFormat.videoFormat = videoFrameList_[ev.frameIdx].format;
				if (curVideoFromPTS != -1) {
					// �f���t�H�[�}�b�g�̕ύX����ԂƂ��Ď�肱�ڂ���
					// AMTSplitter�Ƃ̐����������Ȃ��Ȃ�̂ŋ����I�ɒǉ�
					addSection();
				}
				// �f���t�H�[�}�b�g�̕ύX������D�悳����
				curFromPTS = curVideoFromPTS = dataPTS_[ev.frameIdx];
				break;
			case AUDIO_FORMAT_CHANGED:
				if (ev.audioIdx >= (int)curFormat.audioFormat.size()) {
					THROW(FormatException, "StreamEvent's audioIdx exceeds numAudio of the previous table change event");
				}
				curFormat.audioFormat[ev.audioIdx] = audioFrameList_[ev.frameIdx].format;
				curAudioFormats[ev.audioIdx] = audioFrameList_[ev.frameIdx].format;
				if (curFromPTS == -1) {
					curFromPTS = pts;
				}
				break;
			}
		}
		// �Ō�̋�Ԃ�ǉ�
		if (curFromPTS != -1) {
			addSection();
		}
		startPtsList.push_back(endPTS);
		formatStartIndex_.push_back((int)format_.size());

		// frameSectionId�𐶐�
		std::vector<int> outFormatFrames(format_.size());
		std::vector<int> frameSectionId(videoFrameList_.size());
		for (int i = 0; i < int(videoFrameList_.size()); ++i) {
			double pts = modifiedPTS_[i];
			// ��Ԃ�T��
			int sectionId = int(std::partition_point(startPtsList.begin(), startPtsList.end(),
				[=](double sec) {
				return !(pts < sec);
			}) - startPtsList.begin() - 1);
			if (sectionId >= (int)sectionFormatList.size()) {
				THROWF(RuntimeException, "sectionId exceeds section count (%d >= %d) at frame %d",
					sectionId, (int)sectionFormatList.size(), i);
			}
			frameSectionId[i] = sectionId;
			outFormatFrames[sectionFormatList[sectionId]]++;
		}

		// �Z�N�V�������t�@�C���}�b�s���O�𐶐�
		std::vector<int> sectionFileList(sectionFormatList.size());

		if (splitSub) {
			// ���C���t�H�[�}�b�g�ȊO�͌������Ȃ� //

			int mainFormatId = int(std::max_element(
				outFormatFrames.begin(), outFormatFrames.end()) - outFormatFrames.begin());

			fileFormatStartIndex_.push_back(0);
			for (int i = 0, mainFileId = -1, nextFileId = 0, videoId = 0;
				i < (int)sectionFormatList.size(); ++i)
			{
				int vid = format_[sectionFormatList[i]].videoFileId;
				if (videoId != vid) {
					fileFormatStartIndex_.push_back(nextFileId);
					videoId = vid;
				}
				if (sectionFormatList[i] == mainFormatId) {
					if (mainFileId == -1) {
						mainFileId = nextFileId++;
						fileFormatId_.push_back(mainFormatId);
					}
					sectionFileList[i] = mainFileId;
				}
				else {
					sectionFileList[i] = nextFileId++;
					fileFormatId_.push_back(sectionFormatList[i]);
				}
			}
			fileFormatStartIndex_.push_back((int)fileFormatId_.size());
		}
		else {
			for (int i = 0; i < (int)sectionFormatList.size(); ++i) {
				// �t�@�C���ƃt�H�[�}�b�g�͓���
				sectionFileList[i] = sectionFormatList[i];
			}
			for (int i = 0; i < (int)format_.size(); ++i) {
				// �t�@�C���ƃt�H�[�}�b�g�͍P���ϊ�
				fileFormatId_.push_back(i);
			}
			fileFormatStartIndex_ = formatStartIndex_;
		}

		// frameFormatId_�𐶐�
		frameFormatId_.resize(videoFrameList_.size());
		for (int i = 0; i < int(videoFrameList_.size()); ++i) {
			frameFormatId_[i] = sectionFileList[frameSectionId[i]];
		}

		// �t�B���^�p���̓t���[�����X�g����
		filterFrameList_ = std::vector<std::vector<FilterSourceFrame>>(numVideoFile_);
		for (int videoId = 0; videoId < (int)numVideoFile_; ++videoId) {
			int keyFrame = -1;
			std::vector<FilterSourceFrame>& list = filterFrameList_[videoId];

			const auto& format = format_[formatStartIndex_[videoId]].videoFormat;
			double timePerFrame = format.frameRateDenom * MPEG_CLOCK_HZ / (double)format.frameRateNum;

			for (int i = 0; i < (int)videoFrameList_.size(); ++i) {
				int ordered = ordredVideoFrame_[i];
				int formatId = fileFormatId_[frameFormatId_[ordered]];
				if (format_[formatId].videoFileId == videoId) {

					double mPTS = modifiedPTS_[ordered];
					FileVideoFrameInfo& srcframe = videoFrameList_[ordered];
					if (srcframe.isGopStart) {
						keyFrame = int(list.size());
					}

					// �܂��L�[�t���[�����Ȃ��ꍇ�͎̂Ă�
					if (keyFrame == -1) continue;

					FilterSourceFrame frame;
					frame.halfDelay = false;
					frame.frameIndex = i;
					frame.pts = mPTS;
					frame.frameDuration = timePerFrame; // TODO: VFR�Ή�
					frame.framePTS = (int64_t)mPTS;
					frame.fileOffset = srcframe.fileOffset;
					frame.keyFrame = keyFrame;
					frame.cmType = CMTYPE_NONCM; // �ŏ��͑S��NonCM�ɂ��Ă���

					switch (srcframe.pic) {
					case PIC_FRAME:
					case PIC_TFF:
					case PIC_TFF_RFF:
						list.push_back(frame);
						break;
					case PIC_FRAME_DOUBLING:
						list.push_back(frame);
						frame.pts += timePerFrame;
						list.push_back(frame);
						break;
					case PIC_FRAME_TRIPLING:
						list.push_back(frame);
						frame.pts += timePerFrame;
						list.push_back(frame);
						frame.pts += timePerFrame;
						list.push_back(frame);
						break;
					case PIC_BFF:
						frame.halfDelay = true;
						frame.pts -= timePerFrame / 2;
						list.push_back(frame);
						break;
					case PIC_BFF_RFF:
						frame.halfDelay = true;
						frame.pts -= timePerFrame / 2;
						list.push_back(frame);
						frame.halfDelay = false;
						frame.pts += timePerFrame;
						list.push_back(frame);
						break;
					}
				}
			}
		}

		// indexAudioFrameList_���쐬
		int numMaxAudio = 1;
		for (int i = 0; i < (int)format_.size(); ++i) {
			numMaxAudio = std::max(numMaxAudio, (int)format_[i].audioFormat.size());
		}
		indexAudioFrameList_.resize(numMaxAudio);
		for (int i = 0; i < (int)audioFrameList_.size(); ++i) {
			// �Z�����ăZ�N�V�����Ƃ��ĔF������Ȃ�����������
			// numMaxAudio�𒴂��鉹���f�[�^�����݂���\��������
			// �������𒴂��Ă��鉹���t���[���͖�������
			if (audioFrameList_[i].audioIdx < numMaxAudio) {
				indexAudioFrameList_[audioFrameList_[i].audioIdx].push_back(i);
			}
		}

		// audioFileOffsets_�𐶐�
		audioFileOffsets_.resize(audioFrameList_.size() + 1);
		for (int i = 0; i < (int)audioFrameList_.size(); ++i) {
			audioFileOffsets_[i] = audioFrameList_[i].fileOffset;
		}
		const auto& lastFrame = audioFrameList_.back();
		audioFileOffsets_.back() = lastFrame.fileOffset + lastFrame.codedDataSize;

		// ���ԏ��
		srcTotalDuration_ = dataPTS_.back() - dataPTS_.front();
		if (timeList_.size() > 0) {
			auto ti = timeList_[0];
			// ���b�v�A���E���h���Ă�\��������̂ŏ�ʃr�b�g�͎̂ĂČv�Z
			double diff = (double)(int32_t(ti.first / 300 - dataPTS_.front())) / MPEG_CLOCK_HZ;
			tm t = tm();
			ti.second.getDay(t.tm_year, t.tm_mon, t.tm_mday);
			ti.second.getTime(t.tm_hour, t.tm_min, t.tm_sec);
			// ����
			t.tm_mon -= 1; // ����0�n�܂�Ȃ̂�
			t.tm_year -= 1900; // �N��1900������
			t.tm_hour -= 9; // ���{�Ȃ̂�GMT+9
			t.tm_sec -= (int)std::round(diff); // �ŏ��̃t���[���܂Ŗ߂�
			firstFrameTime_ = _mkgmtime(&t);
		}

    fileDivs_.resize(numVideoFile_);
	}

	void calcSizeAndTime(const std::vector<CMType>& cmtypes)
	{
		// CM��͂��Ȃ��Ƃ���fileDivs_���ݒ肳��Ă��Ȃ��̂ł����Őݒ�
		for (int i = 0; i < numVideoFile_; ++i) {
			auto& divs = fileDivs_[i];
			if (divs.size() == 0) {
				divs.push_back(0);
				divs.push_back((int)filterFrameList_[i].size());
			}
		}

		// �t�@�C�����X�g����
		outFileKeys_.clear();
		for (int video = 0; video < numVideoFile_; ++video) {
			int numEncoders = getNumEncoders(video);
			for (int format = 0; format < numEncoders; ++format) {
				for (int div = 0; div < fileDivs_[video].size() - 1; ++div) {
					for (CMType cmtype : cmtypes) {
						outFileKeys_.push_back(EncodeFileKey(video, format, div, cmtype));
					}
				}
			}
		}

		// �e���ԃt�@�C���̓��̓t�@�C�����ԂƃT�C�Y���v�Z
		filterSrcSize_ = std::vector<int64_t>(numVideoFile_, 0);
		filterSrcDuration_ = std::vector<double>(numVideoFile_, 0);
		std::vector<double> fileFormatDuration(fileFormatId_.size(), 0);
		for (int i = 0; i < (int)videoFrameList_.size(); ++i) {
			int ordered = ordredVideoFrame_[i];
			const auto& frame = videoFrameList_[ordered];
			int fileFormatId = frameFormatId_[ordered];
			int formatId = fileFormatId_[fileFormatId];
			int videoId = format_[formatId].videoFileId;
			int next = (i + 1 < (int)videoFrameList_.size())
				? ordredVideoFrame_[i + 1]
				: -1;
			double duration = getSourceFrameDuration(ordered, next);
			// ���ԃt�@�C�����̃T�C�Y�Ǝ��ԁi�\�[�X�r�b�g���[�g�v�Z�p�j
			filterSrcSize_[videoId] += frame.codedDataSize;
			filterSrcDuration_[videoId] += duration;
			// �t�H�[�}�b�g�i�o�́j���Ƃ̎��ԁi�o�̓t�@�C��������p�j
			fileFormatDuration[fileFormatId] += duration;
		}

		int maxId = (int)(std::max_element(fileFormatDuration.begin(), fileFormatDuration.end()) -
			fileFormatDuration.begin());

		// [�t�H�[�}�b�g(�o��)] -> [�o�͗p�ԍ�] �쐬
		// �ł����Ԃ̒����t�H�[�}�b�g(�o��)���[���B����ȊO�͏��Ԓʂ�
		std::vector<int> formatOutIndex(fileFormatId_.size());
		formatOutIndex[maxId] = 0;
		for (int i = 0, cnt = 1; i < (int)formatOutIndex.size(); ++i) {
			if (i != maxId) {
				formatOutIndex[i] = cnt++;
			}
		}

		// �e�o�̓t�@�C���̃��^�f�[�^���쐬
		for (auto key : outFileKeys_) {
			auto& file = outFiles_[key.key()];
			// [�t�H�[�}�b�g(�o��)��]
			int foramtId = fileFormatStartIndex_[key.video] + key.format;

			file.outKey.video = 0; // �g��Ȃ�
			file.outKey.format = formatOutIndex[foramtId];
			file.outKey.div = key.div;
			file.outKey.cm = (key.cm == cmtypes[0]) ? CMTYPE_BOTH : key.cm;
			file.keyMax.video = 0; // �g��Ȃ�
			file.keyMax.format = (int)fileFormatId_.size();
			file.keyMax.div = (int)fileDivs_[key.video].size() - 1;
			file.keyMax.cm = key.cm; // �g��Ȃ�

			// �t���[�����X�g�쐬
			file.videoFrames.clear();
			const auto& frameList = filterFrameList_[key.video];
			int start = fileDivs_[key.video][key.div];
			int end = fileDivs_[key.video][key.div + 1];
			for (int i = start; i < end; ++i) {
				if (foramtId == frameFormatId_[frameList[i].frameIndex]) {
					if (key.cm == CMTYPE_BOTH || key.cm == frameList[i].cmType) {
						file.videoFrames.push_back(i);
					}
				}
			}

			// ���Ԃ��v�Z
			file.duration = 0;
			for (int i = 0; i < (int)file.videoFrames.size(); ++i) {
				file.duration += frameList[file.videoFrames[i]].frameDuration;
			}
		}

		// ���o�͎���
		outTotalDuration_ = 0;
		for (auto key : outFileKeys_) {
			outTotalDuration_ += outFiles_.at(key.key()).duration;
		}
	}

	template<typename I>
	void makeModifiedPTS(int64_t modifiedFirstPTS, std::vector<double>& modifiedPTS, const std::vector<I>& frames)
	{
		// �O��̃t���[����PTS��6���Ԉȏ�̂��ꂪ����Ɛ����������ł��Ȃ�
		if (frames.size() == 0) return;

		// ���b�v�A���E���h���Ȃ�PTS�𐶐�
		modifiedPTS.resize(frames.size());
		int64_t prevPTS = modifiedFirstPTS;
		for (int i = 0; i < int(frames.size()); ++i) {
			int64_t PTS = frames[i].PTS;
			if (PTS == -1) {
				// PTS���Ȃ�
				THROWF(FormatException,
					"PTS������܂���B�����ł��܂���B %d�t���[����", i);
			}
			int64_t modPTS = prevPTS + int64_t((int32_t(PTS) - int32_t(prevPTS)));
			modifiedPTS[i] = (double)modPTS;
			prevPTS = modPTS;
		}

		// �X�g���[�����߂��Ă���ꍇ�͏����ł��Ȃ��̂ŃG���[�Ƃ���
		for (int i = 1; i < int(frames.size()); ++i) {
			if (modifiedPTS[i] - modifiedPTS[i - 1] < -60 * MPEG_CLOCK_HZ) {
				// 1���ȏ�߂��Ă���
				ctx.incrementCounter(AMT_ERR_NON_CONTINUOUS_PTS);
				ctx.warnF("PTS���߂��Ă��܂��B�����������ł��Ȃ���������܂���B [%d] %.0f -> %.0f",
					i, modifiedPTS[i - 1], modifiedPTS[i]);
			}
		}
	}

	void registerOrGetFormat(OutVideoFormat& format) {
		// ���łɂ���̂���T��
		for (int i = formatStartIndex_.back(); i < (int)format_.size(); ++i) {
			if (isEquealFormat(format_[i], format)) {
				format.formatId = i;
				return;
			}
		}
		// �Ȃ��̂œo�^
		format.formatId = (int)format_.size();
		format_.push_back(format);
	}

	bool isEquealFormat(const OutVideoFormat& a, const OutVideoFormat& b) {
		if (a.videoFormat != b.videoFormat) return false;
		if (isEncodeAudio_) return true;
		if (a.audioFormat.size() != b.audioFormat.size()) return false;
		for (int i = 0; i < (int)a.audioFormat.size(); ++i) {
			if (a.audioFormat[i] != b.audioFormat[i]) {
				return false;
			}
		}
		return true;
	}

	struct AudioState {
		double time = 0; // �ǉ����ꂽ�����t���[���̍��v����
		double lostPts = -1; // �����|�C���g����������PTS�i�\���p�j
		int lastFrame = -1;
	};

	struct OutFileState {
		int formatId; // �f�o�b�O�o�͗p
		double time; // �ǉ����ꂽ�f���t���[���̍��v����
		std::vector<AudioState> audioState;
		FileAudioFrameList audioFrameList;
	};

	AudioDiffInfo initAudioDiffInfo() {
		AudioDiffInfo adiff = AudioDiffInfo();
		adiff.totalSrcFrames = (int)audioFrameList_.size();
		adiff.basePts = dataPTS_[0];
		return adiff;
	}

	// �t�B���^���͂��特���\�z
	AudioDiffInfo genAudioStream()
	{
		// �e�t�@�C���̉����\�z
		for (int v = 0; v < (int)outFileKeys_.size(); ++v) {
			auto key = outFileKeys_[v];
			int formatId = fileFormatStartIndex_[key.video] + key.format;
			auto& file = outFiles_[key.key()];
			const auto& srcFrames = filterFrameList_[key.video];
			const auto& audioFormats = format_[fileFormatId_[formatId]].audioFormat;
			int numAudio = (int)audioFormats.size();
			OutFileState state;
			state.formatId = formatId;
			state.time = 0;
			state.audioState.resize(numAudio);
			state.audioFrameList.resize(numAudio);
			for (int i = 0; i < (int)file.videoFrames.size(); ++i) {
				const auto& frame = srcFrames[file.videoFrames[i]];
				addVideoFrame(state, audioFormats, frame.pts, frame.frameDuration, nullptr);
			}
			file.audioFrames = std::move(state.audioFrameList);
		}

		// ���Y�����v���̂��߂���1�p�X���s
		AudioDiffInfo adiff = initAudioDiffInfo();
		std::vector<OutFileState> states(fileFormatId_.size());
		for (int i = 0; i < (int)states.size(); ++i) {
			int numAudio = (int)format_[fileFormatId_[i]].audioFormat.size();
			states[i].formatId = i;
			states[i].time = 0;
			states[i].audioState.resize(numAudio);
			states[i].audioFrameList.resize(numAudio);
		}
		for (int videoId = 0; videoId < numVideoFile_; ++videoId) {
			const auto& frameList = filterFrameList_[videoId];
			for (int i = 0; i < (int)frameList.size(); ++i) {
				const auto& frame = frameList[i];
				int fileFormatId = frameFormatId_[frame.frameIndex];
				const auto& audioFormats = format_[fileFormatId_[fileFormatId]].audioFormat;
				addVideoFrame(states[fileFormatId],
					audioFormats, frame.pts, frame.frameDuration, &adiff);
			}
		}

		return adiff;
	}

	void genWaveAudioStream()
	{
		// �S�f���t���[����ǉ�
		ctx.info("[CM����p�����\�z]");
		filterAudioFrameList_.resize(numVideoFile_);
		for (int videoId = 0; videoId < (int)numVideoFile_; ++videoId) {
			OutFileState file = { 0 };
			file.formatId = -1;
			file.time = 0;
			file.audioState.resize(1);
			file.audioFrameList.resize(1);

			auto& frames = filterFrameList_[videoId];
			auto& format = format_[formatStartIndex_[videoId]];

			// AviSynth��VFR�ɑΉ����Ă��Ȃ��̂ŁACFR�O��Ŗ��Ȃ�
			double timePerFrame = format.videoFormat.frameRateDenom * MPEG_CLOCK_HZ / (double)format.videoFormat.frameRateNum;

			for (int i = 0; i < (int)frames.size(); ++i) {
				double endPts = frames[i].pts + timePerFrame;
				file.time += timePerFrame;

				// file.time�܂ŉ�����i�߂�
				auto& audioState = file.audioState[0];
				if (audioState.time < file.time) {
					double audioDuration = file.time - audioState.time;
					double audioPts = endPts - audioDuration;
					// �X�e���I�ɕϊ�����Ă���͂��Ȃ̂ŁA�����t�H�[�}�b�g�͖��Ȃ�
					fillAudioFrames(file, 0, nullptr, audioPts, audioDuration, nullptr);
				}
			}

			auto& list = file.audioFrameList[0];
			for (int i = 0; i < (int)list.size(); ++i) {
				FilterAudioFrame frame = { 0 };
				auto& info = audioFrameList_[list[i]];
				frame.frameIndex = list[i];
				frame.waveOffset = info.waveOffset;
				frame.waveLength = info.waveDataSize;
				filterAudioFrameList_[videoId].push_back(frame);
			}
		}
	}

	// �\�[�X�t���[���̕\������
	// index, nextIndex: DTS��
	double getSourceFrameDuration(int index, int nextIndex) {
		const auto& videoFrame = videoFrameList_[index];
		int formatId = fileFormatId_[frameFormatId_[index]];
		const auto& format = format_[formatId];
		double frameDiff = format.videoFormat.frameRateDenom * MPEG_CLOCK_HZ / (double)format.videoFormat.frameRateNum;

		double duration;
		if (isVFR_) { // VFR
			if (nextIndex == -1) {
				duration = 0; // �Ō�̃t���[��
			}
			else {
				duration = modifiedPTS_[nextIndex] - modifiedPTS_[index];
			}
		}
		else { // CFR
			switch (videoFrame.pic) {
			case PIC_FRAME:
			case PIC_TFF:
			case PIC_BFF:
				duration = frameDiff;
				break;
			case PIC_TFF_RFF:
			case PIC_BFF_RFF:
				duration = frameDiff * 1.5;
				hasRFF_ = true;
				break;
			case PIC_FRAME_DOUBLING:
				duration = frameDiff * 2;
				hasRFF_ = true;
				break;
			case PIC_FRAME_TRIPLING:
				duration = frameDiff * 3;
				hasRFF_ = true;
				break;
			}
		}

		return duration;
	}

	void addVideoFrame(OutFileState& file, 
		const std::vector<AudioFormat>& audioFormat,
		double pts, double duration, AudioDiffInfo* adiff)
	{
		double endPts = pts + duration;
		file.time += duration;

		ASSERT(audioFormat.size() == file.audioFrameList.size());
		ASSERT(audioFormat.size() == file.audioState.size());
		for (int i = 0; i < (int)audioFormat.size(); ++i) {
			// file.time�܂ŉ�����i�߂�
			auto& audioState = file.audioState[i];
			if (audioState.time >= file.time) {
				// �����͏\���i��ł�
				continue;
			}
			double audioDuration = file.time - audioState.time;
			double audioPts = endPts - audioDuration;
			const AudioFormat* format = isEncodeAudio_ ? nullptr : &audioFormat[i];
			fillAudioFrames(file, i, format, audioPts, audioDuration, adiff);
		}
	}

	void fillAudioFrames(
		OutFileState& file, int index, // �Ώۃt�@�C���Ɖ����C���f�b�N�X
		const AudioFormat* format, // �����t�H�[�}�b�g
		double pts, double duration, // �J�n�C��PTS��90kHz�ł̃^�C���X�p��
		AudioDiffInfo* adiff)
	{
		auto& state = file.audioState[index];
		const auto& frameList = indexAudioFrameList_[index];

		fillAudioFramesInOrder(file, index, format, pts, duration, adiff);
		if (duration <= 0) {
			// �\���o�͂���
			return;
		}

		// ������������߂����炠�邩������Ȃ��̂ŒT���Ȃ���
		auto it = std::partition_point(frameList.begin(), frameList.end(), [&](int frameIndex) {
			double modPTS = modifiedAudioPTS_[frameIndex];
			double frameDuration = audioFrameDuration_[frameIndex];
			return modPTS + (frameDuration / 2) < pts;
		});
		if (it != frameList.end()) {
			// �������Ƃ���Ɉʒu���Z�b�g���ē���Ă݂�
			if (state.lostPts != pts) {
				state.lostPts = pts;
				if (adiff) {
					auto elapsed = elapsedTime(pts);
					ctx.debugF("%d��%.3f�b�ŉ���%d-%d�̓����|�C���g�����������̂ōČ���",
						elapsed.first, elapsed.second, file.formatId, index);
				}
			}
			state.lastFrame = (int)(it - frameList.begin() - 1);
			fillAudioFramesInOrder(file, index, format, pts, duration, adiff);
		}

		// �L���ȉ����t���[����������Ȃ������ꍇ�͂Ƃ肠�����������Ȃ�
		// ���ɗL���ȉ����t���[�������������炻�̊Ԃ̓t���[�������������
		// �f����艹�����Z���Ȃ�\���͂��邪�A�L���ȉ������Ȃ��̂ł���Ύd���Ȃ���
		// ���Y������킯�ł͂Ȃ��̂Ŗ��Ȃ��Ǝv����

	}

	// lastFrame���珇�ԂɌ��ĉ����t���[��������
	void fillAudioFramesInOrder(
		OutFileState& file, int index, // �Ώۃt�@�C���Ɖ����C���f�b�N�X
		const AudioFormat* format, // �����t�H�[�}�b�g
		double& pts, double& duration, // �J�n�C��PTS��90kHz�ł̃^�C���X�p��
		AudioDiffInfo* adiff)
	{
		auto& state = file.audioState[index];
		auto& outFrameList = file.audioFrameList.at(index);
		const auto& frameList = indexAudioFrameList_[index];
		int nskipped = 0;

		for (int i = state.lastFrame + 1; i < (int)frameList.size(); ++i) {
			int frameIndex = frameList[i];
			const auto& frame = audioFrameList_[frameIndex];
			double modPTS = modifiedAudioPTS_[frameIndex];
			double frameDuration = audioFrameDuration_[frameIndex];
			double halfDuration = frameDuration / 2;
			double quaterDuration = frameDuration / 4;

			if (modPTS >= pts + duration) {
				// �J�n���I�������̏ꍇ
				if (modPTS >= pts + frameDuration - quaterDuration) {
					// �t���[����4����3�ȏ�̃Y���Ă���ꍇ
					// �s���߂�
					break;
				}
			}
			if (modPTS + (frameDuration / 2) < pts) {
				// �O������̂ŃX�L�b�v
				++nskipped;
				continue;
			}
			if (format != nullptr && frame.format != *format) {
				// �t�H�[�}�b�g���Ⴄ�̂ŃX�L�b�v
				continue;
			}

			// �󂫂�����ꍇ�̓t���[���𐅑�������
			// �t���[����4����3�ȏ�̋󂫂��ł���ꍇ�͖��߂�
			int nframes = (int)std::max(1.0, ((modPTS - pts) + (frameDuration / 4)) / frameDuration);

			if (adiff) {
				if (nframes > 1) {
					auto elapsed = elapsedTime(modPTS);
					ctx.debugF("%d��%.3f�b�ŉ���%d-%d�ɂ��ꂪ����̂�%d�t���[��������",
						elapsed.first, elapsed.second, file.formatId, index, nframes - 1);
				}
				if (nskipped > 0) {
					if (state.lastFrame == -1) {
						ctx.debugF("����%d-%d��%d�t���[���ڂ���J�n",
							file.formatId, index, nskipped);
					}
					else {
						auto elapsed = elapsedTime(modPTS);
						ctx.debugF("%d��%.3f�b�ŉ���%d-%d�ɂ��ꂪ����̂�%d�t���[���X�L�b�v",
							elapsed.first, elapsed.second, file.formatId, index, nskipped);
					}
					nskipped = 0;
				}

				++adiff->totalUniquAudioFrames;
			}

			for (int t = 0; t < nframes; ++t) {
				// ���v���
				if (adiff) {
					double diff = std::abs(modPTS - pts);
					if (adiff->maxPtsDiff < diff) {
						adiff->maxPtsDiff = diff;
						adiff->maxPtsDiffPos = pts;
					}
					adiff->sumPtsDiff += diff;
					++adiff->totalAudioFrames;
				}

				// �t���[�����o��
				outFrameList.push_back(frameIndex);
				state.time += frameDuration;
				pts += frameDuration;
				duration -= frameDuration;
			}

			state.lastFrame = i;
			if (duration <= 0) {
				// �\���o�͂���
				return;
			}
		}
	}

	// �t�@�C���S�̂ł̎���
	std::pair<int, double> elapsedTime(double modPTS) const {
		double sec = (double)(modPTS - dataPTS_[0]) / MPEG_CLOCK_HZ;
		int minutes = (int)(sec / 60);
		sec -= minutes * 60;
		return std::make_pair(minutes, sec);
	}

	void genCaptionStream()
	{
		ctx.info("[�����\�z]");

		for (int v = 0; v < (int)outFileKeys_.size(); ++v) {
			auto key = outFileKeys_[v];
			auto& file = outFiles_[key.key()];
			const auto& srcFrames = filterFrameList_[key.video];
			const auto& frames = file.videoFrames;
			std::vector<double> frameTimes;

			auto pred = [&](const int& f, double mid) { return srcFrames[f].pts < mid; };

			auto getFrameIndex = [&](double pts) {
				return std::lower_bound(
					frames.begin(), frames.end(), pts, pred) - frames.begin();
			};

			auto containsPTS = [&](double pts) {
				auto it = std::lower_bound(srcFrames.begin(), srcFrames.end(), pts,
					[](const FilterSourceFrame& frame, double mid) { return frame.pts < mid; });
				if (it != srcFrames.end()) {
					int idx = (int)(it - srcFrames.begin());
					auto it2 = std::lower_bound(frames.begin(), frames.end(), idx);
					if (it2 != frames.end() && *it2 == idx) {
						return true;
					}
				}
				return false;
			};

			double curTime = 0.0;
			for (int i = 0; i < (int)frames.size(); ++i) {
				frameTimes.push_back(curTime);
				curTime += srcFrames[frames[i]].frameDuration;
			}
			// �ŏI�t���[���̏I���������ǉ�
			frameTimes.push_back(curTime);

			// �����𐶐�
			for (int i = 0; i < (int)captionItemList_.size(); ++i) {
				if (captionItemList_[i].line) { // �N���A�ȊO
					auto duration = captionDuration_[i];
					auto start = getFrameIndex(duration.startPTS);
					auto end = getFrameIndex(duration.endPTS);
					if (start < end) { // 1�t���[���ȏ�\�����Ԃ̂���ꍇ�̂�
						int langIndex = captionItemList_[i].langIndex;
						if (langIndex >= file.captionList.size()) { // ���ꂪ����Ȃ��ꍇ�͍L����
							file.captionList.resize(langIndex + 1);
						}
						OutCaptionLine outcap = {
							frameTimes[start], frameTimes[end], captionItemList_[i].line.get()
						};
						file.captionList[langIndex].push_back(outcap);
					}
				}
			}

			// �j�R�j�R�����R�����g�𐶐�
			for (int t = 0; t < NICOJK_MAX; ++t) {
				auto& srcList = nicoJKList_[t];
				for (int i = 0; i < (int)srcList.size(); ++i) {
					auto item = srcList[i];
					// �J�n�����̃t�@�C���Ɋ܂܂�Ă��邩
					if (containsPTS(item.start)) {
						double startTime = frameTimes[getFrameIndex(item.start)];
						double endTime = frameTimes[getFrameIndex(item.end)];
						NicoJKLine outcomment = { startTime, endTime, item.line };
						file.nicojkList[t].push_back(outcomment);
					}
				}
			}
		}
	}
};

