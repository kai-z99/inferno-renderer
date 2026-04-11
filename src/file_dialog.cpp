#include "file_dialog.h"

#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#else
#include <array>
#endif

namespace
{
// Parent directory of the last successfully chosen model file 
std::filesystem::path g_last_model_browse_directory;

std::string path_for_cvar(std::filesystem::path selected)
{
	std::error_code ec;
	std::filesystem::path cwd = std::filesystem::current_path(ec);
	if (!ec)
	{
		std::filesystem::path rel = std::filesystem::relative(selected, cwd, ec);
		if (!ec)
		{
			return rel.generic_string();
		}
	}
	return selected.generic_string();
}

#if !defined(_WIN32)
void append_shell_single_quoted(std::string& out, const std::string& s)
{
	out.push_back('\'');
	for (char c : s)
	{
		if (c == '\'')
		{
			out += "'\\''";
		}
		else
		{
			out.push_back(c);
		}
	}
	out.push_back('\'');
}

std::optional<std::string> run_popen_read_line(const char* cmd)
{
	FILE* pipe = popen(cmd, "r");
	if (!pipe)
	{
		return std::nullopt;
	}
	std::string out;
	std::array<char, 4096> buf{};
	while (fgets(buf.data(), static_cast<int>(buf.size()), pipe))
	{
		out += buf.data();
	}
	const int closeStatus = pclose(pipe);
	if (closeStatus != 0)
	{
		return std::nullopt;
	}
	while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
	{
		out.pop_back();
	}
	if (out.empty())
	{
		return std::nullopt;
	}
	return out;
}
#endif
} // namespace

std::optional<std::string> open_model_file_dialog()
{
	std::error_code fsEc;
	std::filesystem::path startDir;
	if (!g_last_model_browse_directory.empty()
		&& std::filesystem::is_directory(g_last_model_browse_directory, fsEc))
	{
		startDir = g_last_model_browse_directory;
	}

#if defined(_WIN32)
	static std::string win_initial_dir_storage;
	char pathBuf[MAX_PATH]{};
	OPENFILENAMEA ofn{};
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFile = pathBuf;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrFilter = "glTF models\0*.glb;*.gltf\0All files\0*.*\0\0";
	ofn.nFilterIndex = 1;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER;
	ofn.lpstrTitle = "Select model";
	if (!startDir.empty())
	{
		win_initial_dir_storage = startDir.string();
		ofn.lpstrInitialDir = win_initial_dir_storage.c_str();
	}
	else
	{
		ofn.lpstrInitialDir = nullptr;
	}
	if (!GetOpenFileNameA(&ofn))
	{
		return std::nullopt;
	}
	const std::filesystem::path chosen(pathBuf);
	g_last_model_browse_directory = chosen.parent_path();
	return path_for_cvar(chosen);
#else
	std::string cmd =
		"zenity --file-selection --title=\"Select glTF model\" "
		"--file-filter=\"glTF (*.glb *.gltf)|*.glb *.gltf\" "
		"--file-filter=\"All files|*\"";
	if (!startDir.empty())
	{
		cmd += " --filename=";
		std::string dir = startDir.generic_string();
		if (!dir.empty() && dir.back() != '/')
		{
			dir.push_back('/');
		}
		append_shell_single_quoted(cmd, dir);
	}
	if (std::optional<std::string> z = run_popen_read_line(cmd.c_str()))
	{
		const std::filesystem::path chosen(*z);
		g_last_model_browse_directory = chosen.parent_path();
		return path_for_cvar(chosen);
	}
	cmd = "kdialog --getopenfilename ";
	if (!startDir.empty())
	{
		append_shell_single_quoted(cmd, startDir.generic_string());
	}
	else
	{
		cmd += '.';
	}
	cmd += " '*.glb *.gltf|glTF models'";
	if (std::optional<std::string> k = run_popen_read_line(cmd.c_str()))
	{
		const std::filesystem::path chosen(*k);
		g_last_model_browse_directory = chosen.parent_path();
		return path_for_cvar(chosen);
	}
	return std::nullopt;
#endif
}
