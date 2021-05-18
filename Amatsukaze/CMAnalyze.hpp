/**
* Amtasukaze CM Analyze
* Copyright (c) 2017-2019 Nekopanda
*
* This software is released under the MIT License.
* http://opensource.org/licenses/mit-license.php
*/
#pragma once

#include <fstream>
#include <string>
#include <iostream>
#include <memory>
#include <regex>

#include "StreamUtils.hpp"
#include "TranscodeSetting.hpp"
#include "LogoScan.hpp"
#include "ProcessThread.hpp"
#include "PerformanceUtil.hpp"

class CMAnalyze : public AMTObject
{
public:
	CMAnalyze(AMTContext& ctx,
		const ConfigWrapper& setting,
		int videoFileIndex, int numFrames)
		: AMTObject(ctx)
		, setting_(setting)
	{
		Stopwatch sw;
		tstring avspath = makeAVSFile(videoFileIndex);

		// ���S���
		if (setting_.getLogoPath().size() > 0 || setting_.getEraseLogoPath().size() > 0) {
			ctx.info("[���S���]");
			sw.start();
			logoFrame(videoFileIndex, avspath);
			ctx.infoF("����: %.2f�b", sw.getAndReset());

			ctx.info("[���S��͌���]");
			if (logopath.size() > 0) {
				ctx.infoF("�}�b�`�������S: %s", logopath);
				PrintFileAll(setting_.getTmpLogoFramePath(videoFileIndex));
			}
			const auto& eraseLogoPath = setting_.getEraseLogoPath();
			for (int i = 0; i < (int)eraseLogoPath.size(); ++i) {
				ctx.infoF("�ǉ����S%d: %s", i + 1, eraseLogoPath[i]);
				PrintFileAll(setting_.getTmpLogoFramePath(videoFileIndex, i));
			}
		}

		// �`���v�^�[���
		ctx.info("[�����E�V�[���`�F���W���]");
		sw.start();
		chapterExe(videoFileIndex, avspath);
		ctx.infoF("����: %.2f�b", sw.getAndReset());

		ctx.info("[�����E�V�[���`�F���W��͌���]");
		PrintFileAll(setting_.getTmpChapterExeOutPath(videoFileIndex));

		// CM����
		ctx.info("[CM���]");
		sw.start();
		joinLogoScp(videoFileIndex);
		ctx.infoF("����: %.2f�b", sw.getAndReset());

		ctx.info("[CM��͌��� - TrimAVS]");
		PrintFileAll(setting_.getTmpTrimAVSPath(videoFileIndex));
		if (setting.isMakeTrimavs()) {
			// ���Ƀt�@�C�������݂���ꍇ�͏㏑�����Ȃ�.
			auto dstAvsPath = StringFormat(_T("%s%s"), setting_.getSrcFilePath(), _T("trim.avs"));
			if (!fs::exists(dstAvsPath)) {
				fs::copy_file(setting_.getTmpTrimAVSPath(videoFileIndex), dstAvsPath);
			}
		}
		ctx.info("[CM��͌��� - �ڍ�]");
		PrintFileAll(setting_.getTmpJlsPath(videoFileIndex));

		// AVS�t�@�C������CM��Ԃ�ǂ�
		readTrimAVS(videoFileIndex, numFrames);

		// �V�[���`�F���W
		readSceneChanges(videoFileIndex);

		// �������
		readDiv(videoFileIndex, numFrames);

		makeCMZones(numFrames);
	}

	CMAnalyze(AMTContext& ctx,
		const ConfigWrapper& setting)
		: AMTObject(ctx)
		, setting_(setting)
	{ }

	const tstring& getLogoPath() const {
		return logopath;
	}

	const std::vector<int>& getTrims() const {
		return trims;
	}

	const std::vector<EncoderZone>& getZones() const {
		return cmzones;
	}

	const std::vector<int>& getDivs() const {
		return divs;
	}

	// PMT�ύX��񂩂�CM�ǉ��F��
	void applyPmtCut(
		int numFrames, const double* rates,
		const std::vector<int>& pidChanges)
	{
		if (sceneChanges.size() == 0) {
			ctx.info("�V�[���`�F���W��񂪂Ȃ�����PMT�ύX����CM����ɗ��p�ł��܂���");
		}

		ctx.info("[PMT�X�VCM�F��]");

		int validStart = 0, validEnd = numFrames;
		std::vector<int> matchedPoints;

		// picChanges�ɋ߂�sceneChanges��������
		for (int i = 1; i < (int)pidChanges.size(); ++i) {
			int next = (int)(std::lower_bound(
				sceneChanges.begin(), sceneChanges.end(),
				pidChanges[i]) - sceneChanges.begin());
			int prev = next;
			if (next > 0) {
				prev = next - 1;
			}
			if (next == sceneChanges.size()) {
				next = prev;
			}
			//ctx.infoF("%d,%d,%d,%d,%d", pidChanges[i], next, sceneChanges[next], prev, sceneChanges[prev]);
			int diff = std::abs(pidChanges[i] - sceneChanges[next]);
			if (diff < 30 * 2) { // ��
				matchedPoints.push_back(sceneChanges[next]);
				ctx.infoF("�t���[��%d��PMT�ύX�̓t���[��%d�ɃV�[���`�F���W����",
					pidChanges[i], sceneChanges[next]);
			}
			else {
				diff = std::abs(pidChanges[i] - sceneChanges[prev]);
				if (diff < 30 * 2) { // �O
					matchedPoints.push_back(sceneChanges[prev]);
					ctx.infoF("�t���[��%d��PMT�ύX�̓t���[��%d�ɃV�[���`�F���W����",
						pidChanges[i], sceneChanges[prev]);
				}
				else {
					ctx.infoF("�t���[��%d��PMT�ύX�͕t�߂ɃV�[���`�F���W���Ȃ����ߖ������܂�", pidChanges[i]);
				}
			}
		}

		// �O��J�b�g�������Z�o
		int maxCutFrames0 = (int)(rates[0] * numFrames);
		int maxCutFrames1 = numFrames - (int)(rates[1] * numFrames);
		for (int i = 0; i < (int)matchedPoints.size(); ++i) {
			if (matchedPoints[i] < maxCutFrames0) {
				validStart = std::max(validStart, matchedPoints[i]);
			}
			if (matchedPoints[i] > maxCutFrames1) {
				validEnd = std::min(validEnd, matchedPoints[i]);
			}
		}
		ctx.infoF("�ݒ���: 0-%d %d-%d", maxCutFrames0, maxCutFrames1, numFrames);
		ctx.infoF("���oCM���: 0-%d %d-%d", validStart, validEnd, numFrames);

		// trims�ɔ��f
		auto copy = trims;
		trims.clear();
		for (int i = 0; i < (int)copy.size(); i += 2) {
			auto start = copy[i];
			auto end = copy[i + 1];
			if (end <= validStart) {
				// �J�n�O
				continue;
			}
			else if (start <= validStart) {
				// �r������J�n
				start = validStart;
			}
			if (start >= validEnd) {
				// �I����
				continue;
			}
			else if (end >= validEnd) {
				// �r���ŏI��
				end = validEnd;
			}
			trims.push_back(start);
			trims.push_back(end);
		}

		// cmzones�ɔ��f
		makeCMZones(numFrames);
	}

	void inputTrimAVS(int numFrames, const tstring& trimavsPath)
	{
		ctx.infoF("[Trim������]: %s", trimavsPath.c_str());
		PrintFileAll(trimavsPath);

		// AVS�t�@�C������CM��Ԃ�ǂ�
		File file(trimavsPath, _T("r"));
		std::string str;
		if (!file.getline(str)) {
			THROW(FormatException, "TrimAVS�t�@�C�����ǂ߂܂���");
		}
		readTrimAVS(str, numFrames);

		// cmzones�ɔ��f
		makeCMZones(numFrames);
	}

private:
	class MySubProcess : public EventBaseSubProcess {
	public:
		MySubProcess(const tstring& args, File* out = nullptr, File* err = nullptr)
			: EventBaseSubProcess(args)
			, out(out)
			, err(err)
		{ }
	protected:
		File* out;
		File* err;
		virtual void onOut(bool isErr, MemoryChunk mc) {
			// ����̓}���`�X���b�h�ŌĂ΂��̒���
			File* dst = isErr ? err : out;
			if (dst != nullptr) {
				dst->write(mc);
			}
			else {
				fwrite(mc.data, mc.length, 1, SUBPROC_OUT);
				fflush(SUBPROC_OUT);
			}
		}
	};

	const ConfigWrapper& setting_;

	tstring logopath;
	std::vector<int> trims;
	std::vector<EncoderZone> cmzones;
	std::vector<int> sceneChanges;
	std::vector<int> divs;

	tstring makeAVSFile(int videoFileIndex)
	{
		StringBuilder sb;
		
		// �I�[�g���[�h�v���O�C���̃��[�h�Ɏ��s����Ɠ��삵�Ȃ��Ȃ�̂ł�������
		sb.append("ClearAutoloadDirs()\n");

		sb.append("LoadPlugin(\"%s\")\n", GetModulePath());
		sb.append("AMTSource(\"%s\")\n", setting_.getTmpAMTSourcePath(videoFileIndex));
		sb.append("Prefetch(1)\n");
		tstring avspath = setting_.getTmpSourceAVSPath(videoFileIndex);
		File file(avspath, _T("w"));
		file.write(sb.getMC());
		return avspath;
	}

	std::string makePreamble() {
		StringBuilder sb;
		// �V�X�e���̃v���O�C���t�H���_�𖳌���
		if (setting_.isSystemAvsPlugin() == false) {
			sb.append("ClearAutoloadDirs()\n");
		}
		// Amatsukaze�p�I�[�g���[�h�t�H���_��ǉ�
		sb.append("AddAutoloadDir(\"%s/plugins64\")\n", GetModuleDirectory());
		return sb.str();
	}

	void logoFrame(int videoFileIndex, const tstring& avspath)
	{
		bool useCacheFile = setting_.IsUsingCache() && fs::exists(setting_.getTmpBestLogoPath(videoFileIndex));
		if (useCacheFile) {
			File file(setting_.getTmpBestLogoPath(videoFileIndex), _T("rb"));
			logopath = file.readTString();
			return;
		}
		ScriptEnvironmentPointer env = make_unique_ptr(CreateScriptEnvironment2());

		try {
			AVSValue result;
			env->Invoke("Eval", AVSValue(makePreamble().c_str()));
			env->LoadPlugin(to_string(GetModulePath()).c_str(), true, &result);
			PClip clip = env->Invoke("AMTSource", to_string(setting_.getTmpAMTSourcePath(videoFileIndex)).c_str()).AsClip();

			const auto& vi = clip->GetVideoInfo();
			int duration = vi.num_frames * vi.fps_denominator / vi.fps_numerator;

			const auto& logoPath = setting_.getLogoPath();
			const auto& eraseLogoPath = setting_.getEraseLogoPath();

			std::vector<tstring> allLogoPath = logoPath;
			allLogoPath.insert(allLogoPath.end(), eraseLogoPath.begin(), eraseLogoPath.end());
			logo::LogoFrame logof(ctx, allLogoPath, 0.35f);
			logof.scanFrames(clip, env.get());

			if (logoPath.size() > 0) {
#if 0
				logof.dumpResult(setting_.getTmpLogoFramePath(videoFileIndex));
#endif
				logof.selectLogo((int)logoPath.size());
				logof.writeResult(setting_.getTmpLogoFramePath(videoFileIndex));

				float threshold = setting_.isLooseLogoDetection() ? 0.03f : (duration <= 60 * 7) ? 0.03f : 0.1f;
				if (logof.getLogoRatio() < threshold) {
					ctx.info("���̋�Ԃ̓}�b�`���郍�S�͂���܂���ł���");
				}
				else {
					logopath = setting_.getLogoPath()[logof.getBestLogo()];
					File file(setting_.getTmpBestLogoPath(videoFileIndex), _T("wb"));
					file.writeTString(logopath);
				}
			}

			for (int i = 0; i < (int)eraseLogoPath.size(); ++i) {
				logof.writeResult(setting_.getTmpLogoFramePath(videoFileIndex, i), (int)logoPath.size() + i);
			}
		}
		catch (const AvisynthError& avserror) {
			THROWF(AviSynthException, "%s", avserror.msg);
		}
	}

	tstring MakeChapterExeArgs(int videoFileIndex, const tstring& avspath)
	{
		return StringFormat(_T("\"%s\" -v \"%s\" -o \"%s\" %s"),
			setting_.getChapterExePath(), pathToOS(avspath),
			pathToOS(setting_.getTmpChapterExePath(videoFileIndex)),
			setting_.getChapterExeOptions());
	}

	void chapterExe(int videoFileIndex, const tstring& avspath)
	{
		auto args = MakeChapterExeArgs(videoFileIndex, avspath);
		ctx.infoF("%s", args);

		if (setting_.IsUsingCache() && fs::exists(setting_.getTmpChapterExeOutPath(videoFileIndex))) {
			return;
		}
		File stdoutf(setting_.getTmpChapterExeOutPath(videoFileIndex), _T("wb"));
		MySubProcess process(args, &stdoutf);
		int exitCode = process.join();
		if (exitCode != 0) {
			THROWF(FormatException, "ChapterExe���G���[�R�[�h(%d)��Ԃ��܂���", exitCode);
		}
	}

	tstring MakeJoinLogoScpArgs(int videoFileIndex)
	{
		StringBuilderT sb;
		sb.append(_T("\"%s\""), setting_.getJoinLogoScpPath());
		if (logopath.size() > 0) {
			sb.append(_T(" -inlogo \"%s\""), pathToOS(setting_.getTmpLogoFramePath(videoFileIndex)));
		}
		sb.append(_T(" -inscp \"%s\" -incmd \"%s\" -o \"%s\" -oscp \"%s\" -odiv \"%s\" %s"),
			pathToOS(setting_.getTmpChapterExePath(videoFileIndex)),
			pathToOS(setting_.getJoinLogoScpCmdPath()),
			pathToOS(setting_.getTmpTrimAVSPath(videoFileIndex)),
			pathToOS(setting_.getTmpJlsPath(videoFileIndex)),
			pathToOS(setting_.getTmpDivPath(videoFileIndex)),
			setting_.getJoinLogoScpOptions());
		return sb.str();
	}

	void joinLogoScp(int videoFileIndex)
	{
		auto args = MakeJoinLogoScpArgs(videoFileIndex);
		ctx.infoF("%s", args);
		MySubProcess process(args);
		int exitCode = process.join();
		if (exitCode != 0) {
			THROWF(FormatException, "join_logo_scp.exe���G���[�R�[�h(%d)��Ԃ��܂���", exitCode);
		}
	}

	void readTrimAVS(int videoFileIndex, int numFrames)
	{
		File file(setting_.getTmpTrimAVSPath(videoFileIndex), _T("r"));
		std::string str;
		if (!file.getline(str)) {
			THROW(FormatException, "join_logo_scp.exe�̏o��AVS�t�@�C�����ǂ߂܂���");
		}
		readTrimAVS(str, numFrames);
	}

	void readTrimAVS(std::string str, int numFrames)
	{
		std::transform(str.begin(), str.end(), str.begin(), ::tolower);
		std::regex re("trim\\s*\\(\\s*(\\d+)\\s*,\\s*(\\d+)\\s*\\)");
		std::sregex_iterator iter(str.begin(), str.end(), re);
		std::sregex_iterator end;

		trims.clear();
		for (; iter != end; ++iter) {
			trims.push_back(std::stoi((*iter)[1].str()));
			trims.push_back(std::stoi((*iter)[2].str()) + 1);
		}
	}

	void readDiv(int videoFileIndex, int numFrames)
	{
		File file(setting_.getTmpDivPath(videoFileIndex), _T("r"));
		std::string str;
		divs.clear();
		while (file.getline(str)) {
			if (str.size()) {
				divs.push_back(std::atoi(str.c_str()));
			}
		}
		// ���K��
		if (divs.size() == 0) {
			divs.push_back(0);
		}
		if (divs.front() != 0) {
			divs.insert(divs.begin(), 0);
		}
		divs.push_back(numFrames);
	}

	void readSceneChanges(int videoFileIndex)
	{
		File file(setting_.getTmpChapterExeOutPath(videoFileIndex), _T("r"));
		std::string str;

		// �w�b�_�������X�L�b�v
		while (1) {
			if (!file.getline(str)) {
				THROW(FormatException, "ChapterExe.exe�̏o�̓t�@�C�����ǂ߂܂���");
			}
			if (starts_with(str, "----")) {
				break;
			}
		}

		std::regex re0("mute\\s*(\\d+):\\s*(\\d+)\\s*-\\s*(\\d+).*");
		std::regex re1("\\s*SCPos:\\s*(\\d+).*");

		while (file.getline(str)) {
			std::smatch m;
			if (std::regex_search(str, m, re0)) {
				//std::stoi(m[1].str());
				//std::stoi(m[2].str());
			}
			else if (std::regex_search(str, m, re1)) {
				sceneChanges.push_back(std::stoi(m[1].str()));
			}
		}
	}

	void makeCMZones(int numFrames) {
		std::deque<int> split(trims.begin(), trims.end());
		split.push_front(0);
		split.push_back(numFrames);

		for (int i = 1; i < (int)split.size(); ++i) {
			if (split[i] < split[i - 1]) {
				THROW(FormatException, "join_logo_scp.exe�̏o��AVS�t�@�C�����s���ł�");
			}
		}

		cmzones.clear();
		for (int i = 0; i < (int)split.size(); i += 2) {
			EncoderZone zone = { split[i], split[i + 1] };
			if (zone.endFrame - zone.startFrame > 0) {
				cmzones.push_back(zone);
			}
		}
	}
};

class MakeChapter : public AMTObject
{
public:
	MakeChapter(AMTContext& ctx,
		const ConfigWrapper& setting,
		const StreamReformInfo& reformInfo,
		int videoFileIndex,
		const std::vector<int>& trims)
		: AMTObject(ctx)
		, setting(setting)
		, reformInfo(reformInfo)
	{
		makeBase(trims, readJls(setting.getTmpJlsPath(videoFileIndex)));
	}

	void exec(EncodeFileKey key)
	{
		auto filechapters = makeFileChapter(key);
		if (filechapters.size() > 0) {
			writeChapter(filechapters, key);
		}
	}

private:
	struct JlsElement {
		int frameStart;
		int frameEnd;
		int seconds;
		std::string comment;
		bool isCut;
		bool isCM;
		bool isOld;
	};

	const ConfigWrapper& setting;
	const StreamReformInfo& reformInfo;

	std::vector<JlsElement> chapters;

	std::vector<JlsElement> readJls(const tstring& jlspath)
	{
		File file(jlspath, _T("r"));
		std::regex re("^\\s*(\\d+)\\s+(\\d+)\\s+(\\d+)\\s+([-\\d]+)\\s+(\\d+).*:(\\S+)");
		std::regex reOld("^\\s*(\\d+)\\s+(\\d+)\\s+(\\d+)\\s+([-\\d]+)\\s+(\\d+)");
		std::string str;
		std::vector<JlsElement> elements;
		while (file.getline(str)) {
			std::smatch m;
			if (std::regex_search(str, m, re)) {
				JlsElement elem = {
					std::stoi(m[1].str()),
					std::stoi(m[2].str()) + 1,
					std::stoi(m[3].str()),
					m[6].str()
				};
				elements.push_back(elem);
			}
			else if (std::regex_search(str, m, reOld)) {
				JlsElement elem = {
					std::stoi(m[1].str()),
					std::stoi(m[2].str()) + 1,
					std::stoi(m[3].str()),
					""
				};
				elements.push_back(elem);
			}
		}
		return elements;
	}

	static bool startsWith(const std::string& s, const std::string& prefix) {
		auto size = prefix.size();
		if (s.size() < size) return false;
		return std::equal(std::begin(prefix), std::end(prefix), std::begin(s));
	}

	void makeBase(std::vector<int> trims, std::vector<JlsElement> elements)
	{
		// isCut, isCM�t���O�𐶐�
		for (int i = 0; i < (int)elements.size(); ++i) {
			auto& e = elements[i];
			int trimIdx = (int)(std::lower_bound(trims.begin(), trims.end(), (e.frameStart + e.frameEnd) / 2) - trims.begin());
			e.isCut = !(trimIdx % 2);
			e.isCM = (e.comment == "CM");
			e.isOld = (e.comment.size() == 0);
		}

		// �]���Ȃ��̂̓}�[�W
		JlsElement cur = elements[0];
		for (int i = 1; i < (int)elements.size(); ++i) {
			auto& e = elements[i];
			bool isMerge = false;
			if (cur.isCut && e.isCut) {
				if (cur.isCM == e.isCM) {
					isMerge = true;
				}
			}
			if (isMerge) {
				cur.frameEnd = e.frameEnd;
				cur.seconds += e.seconds;
			}
			else {
				chapters.push_back(cur);
				cur = e;
			}
		}
		chapters.push_back(cur);

		// �R�����g���`���v�^�[���ɕύX
		int nChapter = -1;
		bool prevCM = true;
		for (int i = 0; i < (int)chapters.size(); ++i) {
			auto& c = chapters[i];
			if (c.isCut) {
				if (c.isCM || c.isOld) c.comment = "CM";
				else c.comment = "CM?";
				prevCM = true;
			}
			else {
				bool showSec = false;
				if (startsWith(c.comment, "Trailer") ||
					startsWith(c.comment, "Sponsor") ||
					startsWith(c.comment, "Endcard") ||
					startsWith(c.comment, "Edge") ||
					startsWith(c.comment, "Border") ||
					c.seconds == 60 ||
					c.seconds == 90)
				{
					showSec = true;
				}
				if (prevCM) {
					++nChapter;
					prevCM = false;
				}
				c.comment = 'A' + (nChapter % 26);
				if (showSec) {
					c.comment += std::to_string(c.seconds) + "Sec";
				}
			}
		}
	}

	std::vector<JlsElement> makeFileChapter(EncodeFileKey key)
	{
		const auto& outFrames = reformInfo.getEncodeFile(key).videoFrames;

		// �`���v�^�[�𕪊���̃t���[���ԍ��ɕϊ�
		std::vector<JlsElement> cvtChapters;
		for (int i = 0; i < (int)chapters.size(); ++i) {
			const auto& c = chapters[i];
			JlsElement fc = c;
			fc.frameStart = (int)(std::lower_bound(outFrames.begin(), outFrames.end(), c.frameStart) - outFrames.begin());
			fc.frameEnd = (int)(std::lower_bound(outFrames.begin(), outFrames.end(), c.frameEnd) - outFrames.begin());
			cvtChapters.push_back(fc);
		}

		// �Z������`���v�^�[�͏���
		auto& vfmt = reformInfo.getFormat(key).videoFormat;
		int fps = (int)std::round((float)vfmt.frameRateNum / vfmt.frameRateDenom);
		std::vector<JlsElement> fileChapters;
		JlsElement cur = { 0 };
		for (int i = 0; i < (int)cvtChapters.size(); ++i) {
			auto& c = cvtChapters[i];
			if (c.frameEnd - c.frameStart < fps * 2) { // 2�b�ȉ��̃`���v�^�[�͏���
				cur.frameEnd = c.frameEnd;
			}
			else if (cur.comment.size() == 0) {
				// �܂����g�����Ă��Ȃ��ꍇ�͍��̃`���v�^�[������
				int start = cur.frameStart;
				cur = c;
				cur.frameStart = start;
			}
			else {
				// �������g�������Ă���̂ŁA�o��
				fileChapters.push_back(cur);
				cur = c;
			}
		}
		if (cur.comment.size() > 0) {
			fileChapters.push_back(cur);
		}

		return fileChapters;
	}

	void writeChapter(const std::vector<JlsElement>& chapters, EncodeFileKey key)
	{
		auto& vfmt = reformInfo.getFormat(key).videoFormat;
		float frameMs = (float)vfmt.frameRateDenom / vfmt.frameRateNum * 1000.0f;

		ctx.infoF("�t�@�C��: %d-%d-%d %s", key.video, key.format, key.div, CMTypeToString(key.cm));

		StringBuilder sb;
		int sumframes = 0;
		for (int i = 0; i < (int)chapters.size(); ++i) {
			auto& c = chapters[i];

			ctx.infoF("%5d: %s", c.frameStart, c.comment.c_str());

			int ms = (int)std::round(sumframes * frameMs);
			int s = ms / 1000;
			int m = s / 60;
			int h = m / 60;
			int ss = ms % 1000;
			s %= 60;
			m %= 60;
			h %= 60;

			sb.append("CHAPTER%02d=%02d:%02d:%02d.%03d\n", (i + 1), h, m, s, ss);
			sb.append("CHAPTER%02dNAME=%s\n", (i + 1), c.comment);

			sumframes += c.frameEnd - c.frameStart;
		}

		File file(setting.getTmpChapterPath(key), _T("w"));
		file.write(sb.getMC());
	}
};
