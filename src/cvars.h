#pragma once

#include <glm/vec3.hpp>
#include <string>
#include <string_utils.h>

class CVarParameter;

enum class CVarFlags : uint32_t
{
	None = 0,
	Noedit = 1 << 1,
	EditReadOnly = 1 << 2,
	Advanced = 1 << 3,
};

enum class CVarEditHint : uint8_t
{
	TextBox,
	Slider,
	Drag,
	Checkbox,
};

struct FloatCVarOptions
{
	double minValue = 0.0;
	double maxValue = 1.0;
	float step = 0.001f;
	std::string format = "%.3f";
};

struct IntCVarOptions
{
	int32_t minValue = 0;
	int32_t maxValue = 1;
	int32_t step = 1;
	std::string format = "%i";
};

struct StringCVarOptions
{
	uint32_t maxLength = 256;
};

struct Vec3CVarOptions
{
	float minValue = -1.0f;
	float maxValue = 1.0f;
	float step = 0.01f;
	std::string format = "%.3f";
};
class CVarSystem
{

public:
	static CVarSystem* Get();

	//pimpl
	virtual CVarParameter* GetCVar(StringUtils::StringHash hash) = 0;

	virtual double* GetFloatCVar(StringUtils::StringHash hash) = 0;

	virtual int32_t* GetIntCVar(StringUtils::StringHash hash) = 0;

	virtual bool* GetBoolCVar(StringUtils::StringHash hash) = 0;

	virtual glm::vec3* GetVec3CVar(StringUtils::StringHash hash) = 0;

	virtual const char* GetStringCVar(StringUtils::StringHash hash) = 0;

	virtual void SetFloatCVar(StringUtils::StringHash hash, double value) = 0;

	virtual void SetIntCVar(StringUtils::StringHash hash, int32_t value) = 0;

	virtual void SetBoolCVar(StringUtils::StringHash hash, bool value) = 0;

	virtual void SetVec3CVar(StringUtils::StringHash hash, glm::vec3 value) = 0;

	virtual void SetStringCVar(StringUtils::StringHash hash, const char* value) = 0;

	
	virtual CVarParameter* CreateFloatCVar(const char* name, const char* description, double defaultValue, double currentValue, const FloatCVarOptions& options) = 0;
	
	virtual CVarParameter* CreateIntCVar(const char* name, const char* description, int32_t defaultValue, int32_t currentValue, const IntCVarOptions& options) = 0;

	virtual CVarParameter* CreateBoolCVar(const char* name, const char* description, bool defaultValue, bool currentValue) = 0;

	virtual CVarParameter* CreateVec3CVar(const char* name, const char* description, glm::vec3 defaultValue, glm::vec3 currentValue, const Vec3CVarOptions& options) = 0;
	
	virtual CVarParameter* CreateStringCVar(const char* name, const char* description, const char* defaultValue, const char* currentValue, const StringCVarOptions& options) = 0;
	
	virtual void DrawImguiEditor() = 0;
};

template<typename T>
struct AutoCVar
{
protected:
	int index;
	using CVarType = T;
};

struct AutoCVar_Float : AutoCVar<double>
{
	AutoCVar_Float(const char* name, const char* description, double defaultValue, const FloatCVarOptions& options, CVarEditHint editHint, CVarFlags flags = CVarFlags::None);

	double Get();
	double* GetPtr();
	float GetFloat();
	float* GetFloatPtr();
	void Set(double val);
};

struct AutoCVar_Int : AutoCVar<int32_t>
{
	AutoCVar_Int(const char* name, const char* description, int32_t defaultValue, const IntCVarOptions& options, CVarEditHint editHint, CVarFlags flags = CVarFlags::None);
	int32_t Get();
	int32_t* GetPtr();
	void Set(int32_t val);

	void Toggle();
};

struct AutoCVar_String : AutoCVar<std::string>
{
	AutoCVar_String(const char* name, const char* description, const char* defaultValue, const StringCVarOptions& options, CVarEditHint editHint, CVarFlags flags = CVarFlags::None);

	const char* Get();
	void Set(std::string&& val);
};

struct AutoCVar_Bool : AutoCVar<bool>
{
	AutoCVar_Bool(const char* name, const char* description, bool defaultValue, CVarEditHint editHint, CVarFlags flags = CVarFlags::None);

	bool Get();
	bool* GetPtr();
	void Set(bool val);
	void Toggle();
};

struct AutoCVar_Vec3 : AutoCVar<glm::vec3>
{
	AutoCVar_Vec3(const char* name, const char* description, glm::vec3 defaultValue, const Vec3CVarOptions& options, CVarEditHint editHint, CVarFlags flags = CVarFlags::None);

	glm::vec3 Get();
	glm::vec3* GetPtr();
	void Set(glm::vec3 val);
};