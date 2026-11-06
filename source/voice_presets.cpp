#include "voice_presets.h"
#include "preset_format.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace {

using PresetFormat::EndsWithPresetExtension;
using PresetFormat::Lower;
using PresetFormat::Trim;

bool DirectoryExists(const std::string& path) {
#ifdef _WIN32
	const DWORD attributes = GetFileAttributesA(path.c_str());
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
	struct stat info;
	return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
#endif
}

bool CreateDirectoryIfMissing(const std::string& path) {
	if (DirectoryExists(path)) return true;
#ifdef _WIN32
	return CreateDirectoryA(path.c_str(), nullptr) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
#else
	return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
#endif
}

bool IsSafePresetFileName(const std::string& fileName) {
	if (fileName.size() <= 5 || fileName.size() > 128 || !EndsWithPresetExtension(fileName)) return false;
	if (fileName.front() == '.') return false;
	for (unsigned char c : fileName) {
		const bool alphaNumeric = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
			|| (c >= '0' && c <= '9');
		if (!alphaNumeric && c != '_' && c != '-' && c != '.') return false;
	}
	return fileName.find("..") == std::string::npos;
}

std::string JoinPath(const std::string& directory, const std::string& fileName) {
	if (directory.empty()) return fileName;
	const char tail = directory[directory.size() - 1];
	if (tail == '/' || tail == '\\') return directory + fileName;
	return directory + "/" + fileName;
}

} // namespace

VoicePresetRegistry::VoicePresetRegistry() : directory_(DefaultDirectory()) {}

std::string VoicePresetRegistry::DefaultDirectory() {
	const char* candidates[] = {
		"data/gvox/presets",
		"garrysmod/data/gvox/presets"
	};
	for (const char* candidate : candidates) {
		if (DirectoryExists(candidate) && !ListPresetFiles(candidate).empty()) return candidate;
	}
	for (const char* candidate : candidates) {
		if (DirectoryExists(candidate)) return candidate;
	}
	return candidates[0];
}

std::vector<std::string> VoicePresetRegistry::ListPresetFiles(const std::string& directory) {
	std::vector<std::string> result;
#ifdef _WIN32
	WIN32_FIND_DATAA data;
	const std::string pattern = JoinPath(directory, "*.gvox");
	HANDLE handle = FindFirstFileA(pattern.c_str(), &data);
	if (handle != INVALID_HANDLE_VALUE) {
		do {
			if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 && EndsWithPresetExtension(data.cFileName)) {
				result.emplace_back(data.cFileName);
			}
		} while (FindNextFileA(handle, &data));
		FindClose(handle);
	}
#else
	DIR* handle = opendir(directory.c_str());
	if (handle != nullptr) {
		while (dirent* entry = readdir(handle)) {
			const std::string name(entry->d_name);
			if (name != "." && name != ".." && EndsWithPresetExtension(name)) result.push_back(name);
		}
		closedir(handle);
	}
#endif
	std::sort(result.begin(), result.end());
	if (result.size() > 128) result.resize(128);
	return result;
}

int VoicePresetRegistry::StableId(const std::string& fileName, const std::unordered_map<int, size_t>& occupied) {
	uint32_t hash = 2166136261u;
	const std::string normalized = Lower(fileName);
	for (unsigned char c : normalized) {
		hash ^= c;
		hash *= 16777619u;
	}
	const int range = AudioEffects::CUSTOM_ID_MAX - AudioEffects::CUSTOM_ID_MIN + 1;
	int candidate = AudioEffects::CUSTOM_ID_MIN + static_cast<int>(hash % static_cast<uint32_t>(range));
	while (occupied.find(candidate) != occupied.end()) {
		++candidate;
		if (candidate > AudioEffects::CUSTOM_ID_MAX) candidate = AudioEffects::CUSTOM_ID_MIN;
	}
	return candidate;
}

bool VoicePresetRegistry::ParseFile(const std::string& fullPath, const std::string& fileName,
	AudioEffects::Preset& preset, std::string& error) {
	std::ifstream input(fullPath.c_str(), std::ios::binary);
	if (!input) { error = "cannot open file"; return false; }

	input.seekg(0, std::ios::end);
	const std::streamoff size = input.tellg();
	if (size <= 0 || size > 65536) { error = "file is empty or larger than 64 KiB"; return false; }
	input.seekg(0, std::ios::beg);
	std::string content(static_cast<size_t>(size), '\0');
	input.read(&content[0], size);
	if (!input) { error = "cannot read file"; return false; }
	return ParseContent(content, fileName, preset, error);
}

bool VoicePresetRegistry::ParseContent(const std::string& content, const std::string& fileName,
	AudioEffects::Preset& preset, std::string& error) {
	return PresetFormat::Parse(content, fileName, preset, error);
}

int VoicePresetRegistry::Reload(const std::string& requestedDirectory) {
	if (!requestedDirectory.empty()) directory_ = requestedDirectory;
	else directory_ = DefaultDirectory();

	custom_.clear();
	byId_.clear();
	errors_.clear();

	const std::vector<std::string> files = ListPresetFiles(directory_);
	for (const std::string& fileName : files) {
		AudioEffects::Preset preset;
		std::string error;
		if (!ParseFile(JoinPath(directory_, fileName), fileName, preset, error)) {
			errors_.push_back(fileName + ": " + error);
			continue;
		}
		preset.id = StableId(fileName, byId_);
		byId_[preset.id] = custom_.size();
		custom_.push_back(preset);
	}
	return static_cast<int>(custom_.size());
}

int VoicePresetRegistry::ReloadFromMemory(const std::vector<std::pair<std::string, std::string>>& files,
	const std::string& sourceLabel) {
	directory_ = sourceLabel;
	custom_.clear();
	byId_.clear();
	errors_.clear();

	std::vector<std::pair<std::string, std::string>> sorted = files;
	std::sort(sorted.begin(), sorted.end(), [](const auto& left, const auto& right) {
		return left.first < right.first;
	});
	if (sorted.size() > 128) sorted.resize(128);

	for (const auto& file : sorted) {
		AudioEffects::Preset preset;
		std::string error;
		if (!ParseContent(file.second, file.first, preset, error)) {
			errors_.push_back(file.first + ": " + error);
			continue;
		}
		preset.id = StableId(file.first, byId_);
		byId_[preset.id] = custom_.size();
		custom_.push_back(preset);
	}
	return static_cast<int>(custom_.size());
}

bool VoicePresetRegistry::WritePresetFile(const std::string& fileName, const std::string& content,
	std::string& storedPath, std::string& error) {
	storedPath.clear();
	error.clear();
	if (!IsSafePresetFileName(fileName)) {
		error = "filename must contain only letters, digits, dots, underscores or dashes and end in .gvox";
		return false;
	}

	AudioEffects::Preset validated;
	if (!ParseContent(content, fileName, validated, error)) return false;

	std::string directory = DefaultDirectory();
	if (!DirectoryExists(directory)) {
		if (DirectoryExists("garrysmod/data")) {
			if (!CreateDirectoryIfMissing("garrysmod/data/gvox")
				|| !CreateDirectoryIfMissing("garrysmod/data/gvox/presets")) {
				error = "cannot create garrysmod/data/gvox/presets";
				return false;
			}
			directory = "garrysmod/data/gvox/presets";
		} else {
			if (!CreateDirectoryIfMissing("data") || !CreateDirectoryIfMissing("data/gvox")
				|| !CreateDirectoryIfMissing("data/gvox/presets")) {
				error = "cannot create data/gvox/presets";
				return false;
			}
			directory = "data/gvox/presets";
		}
	}

	storedPath = JoinPath(directory, fileName);
	{
		std::ifstream existing(storedPath.c_str(), std::ios::binary);
		if (existing) {
			std::ostringstream current;
			current << existing.rdbuf();
			if (existing.good() || existing.eof()) {
				if (current.str() == content) return true;
			}
		}
	}

	std::ofstream output(storedPath.c_str(), std::ios::binary | std::ios::trunc);
	if (!output) {
		error = "cannot open preset file for writing";
		storedPath.clear();
		return false;
	}
	output.write(content.data(), static_cast<std::streamsize>(content.size()));
	output.close();
	if (!output) {
		error = "cannot write preset file";
		storedPath.clear();
		return false;
	}
	return true;
}

bool VoicePresetRegistry::Find(int id, AudioEffects::Preset& result) const {
	const auto found = byId_.find(id);
	if (found == byId_.end()) return false;
	result = custom_[found->second];
	return true;
}

bool VoicePresetRegistry::IsCustom(int id) const { return byId_.find(id) != byId_.end(); }
const std::vector<AudioEffects::Preset>& VoicePresetRegistry::CustomPresets() const { return custom_; }
const std::vector<std::string>& VoicePresetRegistry::Errors() const { return errors_; }
const std::string& VoicePresetRegistry::Directory() const { return directory_; }
