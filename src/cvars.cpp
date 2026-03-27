#include "cvars.h"


#include <unordered_map>

#include <array>
#include <algorithm>
#include <cstring>
#include <cassert>
#include <cstdio>
#include "imgui.h"
#include "imgui_internal.h"
#include <shared_mutex>
#include <mutex>
#include <vector>

enum class CVarType : char
{
	INT,
	BOOL,
	VEC3,
	FLOAT,
	STRING,
};

class CVarParameter
{
public:
	friend class CVarSystemImpl;

	int32_t arrayIndex;

	CVarType type;
	CVarEditHint editHint = CVarEditHint::TextBox;
	CVarFlags flags;
	std::string name;
	std::string description;
	double minValue = 0.0;
	double maxValue = 1.0;
	float step = 0.001f;
	std::string format = "%.3f";
	float vec3MinValue = -1.0f;
	float vec3MaxValue = 1.0f;
	float vec3Step = 0.01f;
	std::string vec3Format = "%.3f";
	int32_t intMinValue = 0;
	int32_t intMaxValue = 1;
	int32_t intStep = 1;
	std::string intFormat = "%i";
	uint32_t stringMaxLength = 256;
};

template<typename T>
struct CVarStorage
{
	T initial;
	T current;
	CVarParameter* parameter;
};

template<typename T>
struct CVarArray
{
	CVarStorage<T>* cvars{nullptr};
	int32_t lastCVar{ 0 };

	CVarArray(size_t size)
	{
		cvars= new CVarStorage<T>[size]();
	}
	

	CVarStorage<T>* GetCurrentStorage(int32_t index)
	{
		return &cvars[index];
	}

	T* GetCurrentPtr(int32_t index)
	{
		return &cvars[index].current;
	};

	T GetCurrent(int32_t index)
	{
		return cvars[index].current;
	};

	void SetCurrent(const T& val, int32_t index)
	{
		cvars[index].current = val;
	}

	int Add(const T& value, CVarParameter* param)
	{
		int index = lastCVar;

		cvars[index].current = value;
		cvars[index].initial = value;
		cvars[index].parameter = param;

		param->arrayIndex = index;
		lastCVar++; 
		return index;
	}

	int Add(const T& initialValue, const T& currentValue, CVarParameter* param)
	{
		int index = lastCVar;

		cvars[index].current = currentValue;
		cvars[index].initial = initialValue;
		cvars[index].parameter = param;

		param->arrayIndex = index;
		lastCVar++;

		return index;
	}
};

uint32_t Hash(const char* str)
{
	return StringUtils::fnv1a_32(str, strlen(str));
}

class CVarSystemImpl : public CVarSystem
{
public:
	CVarParameter* GetCVar(StringUtils::StringHash hash) override final;

	
	CVarParameter* CreateFloatCVar(const char* name, const char* description, double defaultValue, double currentValue, const FloatCVarOptions& options) override final;
	
	CVarParameter* CreateIntCVar(const char* name, const char* description, int32_t defaultValue, int32_t currentValue, const IntCVarOptions& options) override final;

	CVarParameter* CreateBoolCVar(const char* name, const char* description, bool defaultValue, bool currentValue) override final;

	CVarParameter* CreateVec3CVar(const char* name, const char* description, glm::vec3 defaultValue, glm::vec3 currentValue, const Vec3CVarOptions& options) override final;

	CVarParameter* CreateStringCVar(const char* name, const char* description, const char* defaultValue, const char* currentValue, const StringCVarOptions& options) override final;
	
	double* GetFloatCVar(StringUtils::StringHash hash) override final;
	int32_t* GetIntCVar(StringUtils::StringHash hash) override final;
	bool* GetBoolCVar(StringUtils::StringHash hash) override final;
	glm::vec3* GetVec3CVar(StringUtils::StringHash hash) override final;
	const char* GetStringCVar(StringUtils::StringHash hash) override final;
	

	void SetFloatCVar(StringUtils::StringHash hash, double value) override final;

	void SetIntCVar(StringUtils::StringHash hash, int32_t value) override final;

	void SetBoolCVar(StringUtils::StringHash hash, bool value) override final;

	void SetVec3CVar(StringUtils::StringHash hash, glm::vec3 value) override final;

	void SetStringCVar(StringUtils::StringHash hash, const char* value) override final;

	void DrawImguiEditor() override final;

	void EditParameter(CVarParameter* p, float textWidth);

	constexpr static int MAX_INT_CVARS = 1000;
	CVarArray<int32_t> intCVars2{ MAX_INT_CVARS };

	constexpr static int MAX_BOOL_CVARS = 1000;
	CVarArray<bool> boolCVars{ MAX_BOOL_CVARS };

	constexpr static int MAX_VEC3_CVARS = 1000;
	CVarArray<glm::vec3> vec3CVars{ MAX_VEC3_CVARS };

	constexpr static int MAX_FLOAT_CVARS = 1000;
	CVarArray<double> floatCVars{ MAX_FLOAT_CVARS };

	constexpr static int MAX_STRING_CVARS = 200;
	CVarArray<std::string> stringCVars{ MAX_STRING_CVARS };


	//using templates with specializations to get the cvar arrays for each type.
	//if you try to use a type that doesnt have specialization, it will trigger a linker error
	template<typename T>
	CVarArray<T>* GetCVarArray();

	//templated get-set cvar versions for syntax sugar
	template<typename T>
	T* GetCVarCurrent(uint32_t namehash) {
		CVarParameter* par = GetCVar(namehash);
		if (!par) {
			return nullptr;
		}
		else {
			return GetCVarArray<T>()->GetCurrentPtr(par->arrayIndex);
		}
	}

	template<typename T>
	void SetCVarCurrent(uint32_t namehash, const T& value)
	{
		CVarParameter* cvar = GetCVar(namehash);
		if (cvar)
		{
			GetCVarArray<T>()->SetCurrent(value, cvar->arrayIndex);
		}
	}

	static CVarSystemImpl* Get()
	{
		return static_cast<CVarSystemImpl*>(CVarSystem::Get());
	}

private:

	std::shared_mutex mutex_;

	CVarParameter* InitCVar(const char* name, const char* description);

	std::unordered_map<uint32_t, CVarParameter> savedCVars;

	std::vector<CVarParameter*> cachedEditParameters;
};

double ClampFloatValue(const CVarParameter* parameter, double value)
{
	return std::clamp(value, parameter->minValue, parameter->maxValue);
}

const char* GetFloatFormat(const CVarParameter* parameter)
{
	if (parameter->format.empty())
	{
		return "%.3f";
	}
	return parameter->format.c_str();
}

int32_t ClampIntValue(const CVarParameter* parameter, int32_t value)
{
	return std::clamp(value, parameter->intMinValue, parameter->intMaxValue);
}

const char* GetIntFormat(const CVarParameter* parameter)
{
	if (parameter->intFormat.empty())
	{
		return "%i";
	}
	return parameter->intFormat.c_str();
}

std::string ClampStringValue(const CVarParameter* parameter, const std::string& value)
{
	const size_t maxLength = static_cast<size_t>(std::max<uint32_t>(1, parameter->stringMaxLength));
	if (value.size() <= maxLength)
	{
		return value;
	}
	return value.substr(0, maxLength);
}

glm::vec3 ClampVec3Value(const CVarParameter* parameter, const glm::vec3& value)
{
	glm::vec3 clamped;
	clamped.x = std::clamp(value.x, parameter->vec3MinValue, parameter->vec3MaxValue);
	clamped.y = std::clamp(value.y, parameter->vec3MinValue, parameter->vec3MaxValue);
	clamped.z = std::clamp(value.z, parameter->vec3MinValue, parameter->vec3MaxValue);
	return clamped;
}

bool IsEditHintCompatible(CVarType type, CVarEditHint hint)
{
	switch (type)
	{
	case CVarType::FLOAT:
		return hint == CVarEditHint::TextBox || hint == CVarEditHint::Slider || hint == CVarEditHint::Drag;
	case CVarType::INT:
		return hint == CVarEditHint::TextBox || hint == CVarEditHint::Checkbox || hint == CVarEditHint::Slider || hint == CVarEditHint::Drag;
	case CVarType::BOOL:
		return hint == CVarEditHint::Checkbox;
	case CVarType::VEC3:
		return hint == CVarEditHint::TextBox || hint == CVarEditHint::Slider || hint == CVarEditHint::Drag;
	case CVarType::STRING:
		return hint == CVarEditHint::TextBox;
	default:
		return false;
	}
}

void ValidateEditHintCompatibility(const CVarParameter* parameter)
{
	if (!IsEditHintCompatible(parameter->type, parameter->editHint))
	{
		std::fprintf(stderr, "Invalid CVar edit hint for '%s'\n", parameter->name.c_str());
	}
	assert(IsEditHintCompatible(parameter->type, parameter->editHint) && "CVar edit hint is not compatible with its value type.");
}

template<>
CVarArray<int32_t>* CVarSystemImpl::GetCVarArray<int32_t>()
{
	return &intCVars2;
}

template<>
CVarArray<bool>* CVarSystemImpl::GetCVarArray<bool>()
{
	return &boolCVars;
}

template<>
CVarArray<glm::vec3>* CVarSystemImpl::GetCVarArray<glm::vec3>()
{
	return &vec3CVars;
}

template<>
CVarArray<double>* CVarSystemImpl::GetCVarArray<double>()
{
	return &floatCVars;
}

template<>
CVarArray<std::string>* CVarSystemImpl::GetCVarArray<std::string>()
{
	return &stringCVars;
}



double* CVarSystemImpl::GetFloatCVar(StringUtils::StringHash hash)
{
	return GetCVarCurrent<double>(hash);
}

int32_t* CVarSystemImpl::GetIntCVar(StringUtils::StringHash hash)
{
	return GetCVarCurrent<int32_t>(hash);
}

bool* CVarSystemImpl::GetBoolCVar(StringUtils::StringHash hash)
{
	return GetCVarCurrent<bool>(hash);
}

glm::vec3* CVarSystemImpl::GetVec3CVar(StringUtils::StringHash hash)
{
	return GetCVarCurrent<glm::vec3>(hash);
}

const char* CVarSystemImpl::GetStringCVar(StringUtils::StringHash hash)
{
	return GetCVarCurrent<std::string>(hash)->c_str();
}


CVarSystem* CVarSystem::Get()
{
	static CVarSystemImpl cvarSys{};
	return &cvarSys;
}


CVarParameter* CVarSystemImpl::GetCVar(StringUtils::StringHash hash)
{
	std::shared_lock lock(mutex_);
	auto it = savedCVars.find(hash);

	if (it != savedCVars.end())
	{
		return &(*it).second;
	}

	return nullptr;
}

void CVarSystemImpl::SetFloatCVar(StringUtils::StringHash hash, double value)
{
	CVarParameter* cvar = GetCVar(hash);
	if (!cvar || cvar->type != CVarType::FLOAT)
	{
		return;
	}

	GetCVarArray<double>()->SetCurrent(ClampFloatValue(cvar, value), cvar->arrayIndex);
}

void CVarSystemImpl::SetIntCVar(StringUtils::StringHash hash, int32_t value)
{
	CVarParameter* cvar = GetCVar(hash);
	if (!cvar || cvar->type != CVarType::INT)
	{
		return;
	}

	GetCVarArray<int32_t>()->SetCurrent(ClampIntValue(cvar, value), cvar->arrayIndex);
}

void CVarSystemImpl::SetBoolCVar(StringUtils::StringHash hash, bool value)
{
	CVarParameter* cvar = GetCVar(hash);
	if (!cvar || cvar->type != CVarType::BOOL)
	{
		return;
	}

	GetCVarArray<bool>()->SetCurrent(value, cvar->arrayIndex);
}

void CVarSystemImpl::SetVec3CVar(StringUtils::StringHash hash, glm::vec3 value)
{
	CVarParameter* cvar = GetCVar(hash);
	if (!cvar || cvar->type != CVarType::VEC3)
	{
		return;
	}

	GetCVarArray<glm::vec3>()->SetCurrent(ClampVec3Value(cvar, value), cvar->arrayIndex);
}

void CVarSystemImpl::SetStringCVar(StringUtils::StringHash hash, const char* value)
{
	CVarParameter* cvar = GetCVar(hash);
	if (!cvar || cvar->type != CVarType::STRING)
	{
		return;
	}

	GetCVarArray<std::string>()->SetCurrent(ClampStringValue(cvar, value ? value : ""), cvar->arrayIndex);
}


CVarParameter* CVarSystemImpl::CreateFloatCVar(const char* name, const char* description, double defaultValue, double currentValue, const FloatCVarOptions& options)
{
	std::unique_lock lock(mutex_);
	CVarParameter* param = InitCVar(name, description);
	if (!param) return nullptr;

	param->type = CVarType::FLOAT;
	param->minValue = std::min(options.minValue, options.maxValue);
	param->maxValue = std::max(options.minValue, options.maxValue);
	param->step = options.step > 0.0f ? options.step : 0.001f;
	param->format = options.format.empty() ? "%.3f" : options.format;

	GetCVarArray<double>()->Add(ClampFloatValue(param, defaultValue), ClampFloatValue(param, currentValue), param);

	return param;
}



CVarParameter* CVarSystemImpl::CreateIntCVar(const char* name, const char* description, int32_t defaultValue, int32_t currentValue, const IntCVarOptions& options)
{
	std::unique_lock lock(mutex_);
	CVarParameter* param = InitCVar(name, description);
	if (!param) return nullptr;

	param->type = CVarType::INT;
	param->intMinValue = std::min(options.minValue, options.maxValue);
	param->intMaxValue = std::max(options.minValue, options.maxValue);
	param->intStep = std::max(1, options.step);
	param->intFormat = options.format.empty() ? "%i" : options.format;

	GetCVarArray<int32_t>()->Add(ClampIntValue(param, defaultValue), ClampIntValue(param, currentValue), param);

	return param;
}

CVarParameter* CVarSystemImpl::CreateBoolCVar(const char* name, const char* description, bool defaultValue, bool currentValue)
{
	std::unique_lock lock(mutex_);
	CVarParameter* param = InitCVar(name, description);
	if (!param) return nullptr;

	param->type = CVarType::BOOL;

	GetCVarArray<bool>()->Add(defaultValue, currentValue, param);

	return param;
}

CVarParameter* CVarSystemImpl::CreateVec3CVar(const char* name, const char* description, glm::vec3 defaultValue, glm::vec3 currentValue, const Vec3CVarOptions& options)
{
	std::unique_lock lock(mutex_);
	CVarParameter* param = InitCVar(name, description);
	if (!param) return nullptr;

	param->type = CVarType::VEC3;
	param->vec3MinValue = std::min(options.minValue, options.maxValue);
	param->vec3MaxValue = std::max(options.minValue, options.maxValue);
	param->vec3Step = options.step > 0.0f ? options.step : 0.01f;
	param->vec3Format = options.format.empty() ? "%.3f" : options.format;

	GetCVarArray<glm::vec3>()->Add(
		ClampVec3Value(param, defaultValue),
		ClampVec3Value(param, currentValue),
		param);

	return param;
}



CVarParameter* CVarSystemImpl::CreateStringCVar(const char* name, const char* description, const char* defaultValue, const char* currentValue, const StringCVarOptions& options)
{
	std::unique_lock lock(mutex_);
	CVarParameter* param = InitCVar(name, description);
	if (!param) return nullptr;

	param->type = CVarType::STRING;
	param->stringMaxLength = std::max<uint32_t>(1, options.maxLength);

	GetCVarArray<std::string>()->Add(
		ClampStringValue(param, defaultValue ? defaultValue : ""),
		ClampStringValue(param, currentValue ? currentValue : ""),
		param);

	return param;
}

CVarParameter* CVarSystemImpl::InitCVar(const char* name, const char* description)
{
	
	uint32_t namehash = StringUtils::StringHash{ name };
	savedCVars[namehash] = CVarParameter{};

	CVarParameter& newParam = savedCVars[namehash];

	newParam.name = name;
	newParam.description = description;

	return &newParam;
}

AutoCVar_Float::AutoCVar_Float(const char* name, const char* description, double defaultValue, const FloatCVarOptions& options, CVarEditHint editHint, CVarFlags flags)
{
	CVarParameter* cvar = CVarSystem::Get()->CreateFloatCVar(name, description, defaultValue, defaultValue, options);
	cvar->editHint = editHint;
	cvar->flags = flags;
	ValidateEditHintCompatibility(cvar);
	index = cvar->arrayIndex;
}

template<typename T>
T GetCVarCurrentByIndex(int32_t index) {
	return CVarSystemImpl::Get()->GetCVarArray<T>()->GetCurrent(index);
}
template<typename T>
T* PtrGetCVarCurrentByIndex(int32_t index) {
	return CVarSystemImpl::Get()->GetCVarArray<T>()->GetCurrentPtr(index);
}


template<typename T>
void SetCVarCurrentByIndex(int32_t index,const T& data) {
	CVarSystemImpl::Get()->GetCVarArray<T>()->SetCurrent(data, index);
}


double AutoCVar_Float::Get()
{
	return GetCVarCurrentByIndex<CVarType>(index);
}

double* AutoCVar_Float::GetPtr()
{
	return PtrGetCVarCurrentByIndex<CVarType>(index);
}

float AutoCVar_Float::GetFloat()
{
	return static_cast<float>(Get());
}

float* AutoCVar_Float::GetFloatPtr()
{
	float* result = reinterpret_cast<float*>(GetPtr());
	return result;
}

void AutoCVar_Float::Set(double f)
{
	CVarStorage<double>* storage = CVarSystemImpl::Get()->GetCVarArray<double>()->GetCurrentStorage(index);
	if (storage && storage->parameter)
	{
		f = ClampFloatValue(storage->parameter, f);
	}
	SetCVarCurrentByIndex<CVarType>(index, f);
}

AutoCVar_Int::AutoCVar_Int(const char* name, const char* description, int32_t defaultValue, const IntCVarOptions& options, CVarEditHint editHint, CVarFlags flags)
{
	CVarParameter* cvar = CVarSystem::Get()->CreateIntCVar(name, description, defaultValue, defaultValue, options);
	cvar->editHint = editHint;
	cvar->flags = flags;
	ValidateEditHintCompatibility(cvar);
	index = cvar->arrayIndex;
}

int32_t AutoCVar_Int::Get()
{
	return GetCVarCurrentByIndex<CVarType>(index);
}

int32_t* AutoCVar_Int::GetPtr()
{
	return PtrGetCVarCurrentByIndex<CVarType>(index);
}

void AutoCVar_Int::Set(int32_t val)
{
	CVarStorage<int32_t>* storage = CVarSystemImpl::Get()->GetCVarArray<int32_t>()->GetCurrentStorage(index);
	if (storage && storage->parameter)
	{
		val = ClampIntValue(storage->parameter, val);
	}
	SetCVarCurrentByIndex<CVarType>(index, val);
}

void AutoCVar_Int::Toggle()
{
	bool enabled = Get() != 0;

	Set(enabled ? 0 : 1);
}

AutoCVar_Bool::AutoCVar_Bool(const char* name, const char* description, bool defaultValue, CVarEditHint editHint, CVarFlags flags)
{
	CVarParameter* cvar = CVarSystem::Get()->CreateBoolCVar(name, description, defaultValue, defaultValue);
	cvar->editHint = editHint;
	cvar->flags = flags;
	ValidateEditHintCompatibility(cvar);
	index = cvar->arrayIndex;
}

bool AutoCVar_Bool::Get()
{
	return GetCVarCurrentByIndex<CVarType>(index);
}

bool* AutoCVar_Bool::GetPtr()
{
	return PtrGetCVarCurrentByIndex<CVarType>(index);
}

void AutoCVar_Bool::Set(bool val)
{
	SetCVarCurrentByIndex<CVarType>(index, val);
}

void AutoCVar_Bool::Toggle()
{
	Set(!Get());
}

AutoCVar_Vec3::AutoCVar_Vec3(const char* name, const char* description, glm::vec3 defaultValue, const Vec3CVarOptions& options, CVarEditHint editHint, CVarFlags flags)
{
	CVarParameter* cvar = CVarSystem::Get()->CreateVec3CVar(name, description, defaultValue, defaultValue, options);
	cvar->editHint = editHint;
	cvar->flags = flags;
	ValidateEditHintCompatibility(cvar);
	index = cvar->arrayIndex;
}

glm::vec3 AutoCVar_Vec3::Get()
{
	return GetCVarCurrentByIndex<CVarType>(index);
}

glm::vec3* AutoCVar_Vec3::GetPtr()
{
	return PtrGetCVarCurrentByIndex<CVarType>(index);
}

void AutoCVar_Vec3::Set(glm::vec3 val)
{
	CVarStorage<glm::vec3>* storage = CVarSystemImpl::Get()->GetCVarArray<glm::vec3>()->GetCurrentStorage(index);
	if (storage && storage->parameter)
	{
		val = ClampVec3Value(storage->parameter, val);
	}
	SetCVarCurrentByIndex<CVarType>(index, val);
}

AutoCVar_String::AutoCVar_String(const char* name, const char* description, const char* defaultValue, const StringCVarOptions& options, CVarEditHint editHint, CVarFlags flags)
{
	CVarParameter* cvar = CVarSystem::Get()->CreateStringCVar(name, description, defaultValue, defaultValue, options);
	cvar->editHint = editHint;
	cvar->flags = flags;
	ValidateEditHintCompatibility(cvar);
	index = cvar->arrayIndex;
}

const char* AutoCVar_String::Get()
{
	return GetCVarCurrentByIndex<CVarType>(index).c_str();
};

void AutoCVar_String::Set(std::string&& val)
{
	CVarStorage<std::string>* storage = CVarSystemImpl::Get()->GetCVarArray<std::string>()->GetCurrentStorage(index);
	if (storage && storage->parameter)
	{
		val = ClampStringValue(storage->parameter, val);
	}
	SetCVarCurrentByIndex<CVarType>(index,val);
}


void CVarSystemImpl::DrawImguiEditor()
{
	static std::array<char, 256> searchTextBuffer{};
	ImGui::InputText("Filter", searchTextBuffer.data(), searchTextBuffer.size());
	static bool bShowAdvanced = false;
	ImGui::Checkbox("Advanced", &bShowAdvanced);
	ImGui::Separator();
	cachedEditParameters.clear();

	auto addToEditList = [&](auto parameter)
	{
		bool bHidden = ((uint32_t)parameter->flags & (uint32_t)CVarFlags::Noedit);
		bool bIsAdvanced = ((uint32_t)parameter->flags & (uint32_t)CVarFlags::Advanced);

		if (!bHidden)
		{
			if (!(!bShowAdvanced && bIsAdvanced) && parameter->name.find(searchTextBuffer.data()) != std::string::npos)
			{
				cachedEditParameters.push_back(parameter);
			};
		}
	};

	for (int i = 0; i < GetCVarArray<int32_t>()->lastCVar; i++)
	{
		addToEditList(GetCVarArray<int32_t>()->cvars[i].parameter);
	}
	for (int i = 0; i < GetCVarArray<bool>()->lastCVar; i++)
	{
		addToEditList(GetCVarArray<bool>()->cvars[i].parameter);
	}
	for (int i = 0; i < GetCVarArray<glm::vec3>()->lastCVar; i++)
	{
		addToEditList(GetCVarArray<glm::vec3>()->cvars[i].parameter);
	}
	for (int i = 0; i < GetCVarArray<double>()->lastCVar; i++)
	{
		addToEditList(GetCVarArray<double>()->cvars[i].parameter);
	}
	for (int i = 0; i < GetCVarArray<std::string>()->lastCVar; i++)
	{
		addToEditList(GetCVarArray<std::string>()->cvars[i].parameter);
	}
	
	if (cachedEditParameters.size() > 10)
	{
		std::unordered_map<std::string, std::vector<CVarParameter*>> categorizedParams;

		//insert all the edit parameters into the hashmap by category
		for (auto p : cachedEditParameters)
		{
			int dotPos = -1;
			//find where the first dot is to categorize
			for (int i = 0; i < p->name.length(); i++)
			{
				if (p->name[i] == '.')
				{
					dotPos = i;
					break;
				}
			}
			std::string category = "";
			if (dotPos != -1)
			{
				category = p->name.substr(0, dotPos);
			}

			auto it = categorizedParams.find(category);
			if (it == categorizedParams.end())
			{
				categorizedParams[category] = std::vector<CVarParameter*>();
				it = categorizedParams.find(category);
			}
			it->second.push_back(p);
		}

		for (auto [category, parameters] : categorizedParams)
		{
			//alphabetical sort
			std::sort(parameters.begin(), parameters.end(), [](CVarParameter* A, CVarParameter* B)
				{
					return A->name < B->name;
				});

			if (ImGui::BeginMenu(category.c_str()))
			{
				float maxTextWidth = 0;

				for (auto p : parameters)
				{
					maxTextWidth = std::max(maxTextWidth, ImGui::CalcTextSize(p->name.c_str()).x);
				}
				for (auto p : parameters)
				{
					EditParameter(p, maxTextWidth);
				}

				ImGui::EndMenu();
			}
		}
	}
	else
	{
		//alphabetical sort
		std::sort(cachedEditParameters.begin(), cachedEditParameters.end(), [](CVarParameter* A, CVarParameter* B)
			{
				return A->name < B->name;
			});
		float maxTextWidth = 0;
		for (auto p : cachedEditParameters)
		{
			maxTextWidth = std::max(maxTextWidth, ImGui::CalcTextSize(p->name.c_str()).x);
		}
		for (auto p : cachedEditParameters)
		{
			EditParameter(p, maxTextWidth);
		}
	}
}
void Label(const char* label, float textWidth, float editorWidth = 100.0f)
{
	constexpr float Slack = 50;

	ImGuiWindow* window = ImGui::GetCurrentWindow();
	const ImVec2 lineStart = ImGui::GetCursorScreenPos();
	const ImGuiStyle& style = ImGui::GetStyle();
	float fullWidth = textWidth + Slack;

	ImVec2 textSize = ImGui::CalcTextSize(label);

	ImVec2 startPos = ImGui::GetCursorScreenPos();

	ImGui::Text("%s", label);

	ImVec2 finalPos = { startPos.x + fullWidth, startPos.y };

	ImGui::SameLine();
	ImGui::SetCursorScreenPos(finalPos);

	ImGui::SetNextItemWidth(editorWidth);
}
void CVarSystemImpl::EditParameter(CVarParameter* p, float textWidth)
{
	const bool readonlyFlag = ((uint32_t)p->flags & (uint32_t)CVarFlags::EditReadOnly);
	ValidateEditHintCompatibility(p);

	
	switch (p->type)
	{
	case CVarType::INT:

		if (readonlyFlag)
		{
			std::string displayFormat = p->name + "= " + GetIntFormat(p);
			ImGui::Text(displayFormat.c_str(), GetCVarArray<int32_t>()->GetCurrent(p->arrayIndex));
		}
		else
		{
			if (p->editHint == CVarEditHint::Checkbox)
			{
				bool bCheckbox = GetCVarArray<int32_t>()->GetCurrent(p->arrayIndex) != 0;
				Label(p->name.c_str(), textWidth);
				
				ImGui::PushID(p->name.c_str());

				if (ImGui::Checkbox("", &bCheckbox))
				{
					GetCVarArray<int32_t>()->SetCurrent(ClampIntValue(p, bCheckbox ? 1 : 0), p->arrayIndex);
				}
				ImGui::PopID();
			}
			else
			{
				if (p->editHint == CVarEditHint::TextBox)
				{
					Label(p->name.c_str(), textWidth);
					ImGui::PushID(p->name.c_str());
					int32_t value = GetCVarArray<int32_t>()->GetCurrent(p->arrayIndex);
					const int32_t step = std::max(1, p->intStep);
					const int32_t stepFast = std::max(step, step * 10);
					if (ImGui::InputInt("", &value, step, stepFast))
					{
						GetCVarArray<int32_t>()->SetCurrent(ClampIntValue(p, value), p->arrayIndex);
					}
					ImGui::PopID();
				}
				else if (p->editHint == CVarEditHint::Slider)
				{
					Label(p->name.c_str(), textWidth);
					ImGui::PushID(p->name.c_str());
					int32_t value = GetCVarArray<int32_t>()->GetCurrent(p->arrayIndex);
					if (ImGui::SliderInt("", &value, p->intMinValue, p->intMaxValue, GetIntFormat(p)))
					{
						GetCVarArray<int32_t>()->SetCurrent(ClampIntValue(p, value), p->arrayIndex);
					}
					ImGui::PopID();
				}
				else if (p->editHint == CVarEditHint::Drag)
				{
					Label(p->name.c_str(), textWidth);
					ImGui::PushID(p->name.c_str());
					int32_t value = GetCVarArray<int32_t>()->GetCurrent(p->arrayIndex);
					if (ImGui::DragInt("", &value, (float)p->intStep, p->intMinValue, p->intMaxValue, GetIntFormat(p)))
					{
						GetCVarArray<int32_t>()->SetCurrent(ClampIntValue(p, value), p->arrayIndex);
					}
					ImGui::PopID();
				}
			}
		}
		break;

	case CVarType::BOOL:

		if (readonlyFlag)
		{
			std::string displayFormat = p->name + "= %s";
			ImGui::Text(displayFormat.c_str(), GetCVarArray<bool>()->GetCurrent(p->arrayIndex) ? "true" : "false");
		}
		else
		{
			bool bCheckbox = GetCVarArray<bool>()->GetCurrent(p->arrayIndex);
			Label(p->name.c_str(), textWidth);
			ImGui::PushID(p->name.c_str());
			if (ImGui::Checkbox("", &bCheckbox))
			{
				GetCVarArray<bool>()->SetCurrent(bCheckbox, p->arrayIndex);
			}
			ImGui::PopID();
		}
		break;

	case CVarType::VEC3:

		if (readonlyFlag)
		{
			glm::vec3 value = GetCVarArray<glm::vec3>()->GetCurrent(p->arrayIndex);
			ImGui::Text("%s= (%.3f, %.3f, %.3f)", p->name.c_str(), value.x, value.y, value.z);
		}
		else
		{
			Label(p->name.c_str(), textWidth, 280.0f);
			ImGui::PushID(p->name.c_str());
			glm::vec3 currentValue = GetCVarArray<glm::vec3>()->GetCurrent(p->arrayIndex);
			float value[3] = { currentValue.x, currentValue.y, currentValue.z };
			bool edited = false;
			if (p->editHint == CVarEditHint::Slider)
			{
				edited = ImGui::SliderFloat3("", value, p->vec3MinValue, p->vec3MaxValue, p->vec3Format.c_str());
			}
			else if (p->editHint == CVarEditHint::Drag)
			{
				edited = ImGui::DragFloat3("", value, p->vec3Step, p->vec3MinValue, p->vec3MaxValue, p->vec3Format.c_str());
			}
			else if (p->editHint == CVarEditHint::TextBox)
			{
				edited = ImGui::InputFloat3("", value, p->vec3Format.c_str());
			}
			if (edited)
			{
				glm::vec3 newValue{ value[0], value[1], value[2] };
				GetCVarArray<glm::vec3>()->SetCurrent(ClampVec3Value(p, newValue), p->arrayIndex);
			}
			ImGui::PopID();
		}
		break;

	case CVarType::FLOAT:

		if (readonlyFlag)
		{
			std::string displayFormat = p->name + "= " + GetFloatFormat(p);
			ImGui::Text(displayFormat.c_str(), GetCVarArray<double>()->GetCurrent(p->arrayIndex));
		}
		else
		{
			Label(p->name.c_str(), textWidth);
			ImGui::PushID(p->name.c_str());
			double* currentValue = GetCVarArray<double>()->GetCurrentPtr(p->arrayIndex);
			constexpr double kZeroStep = 0.0;
			const double* minPtr = &p->minValue;
			const double* maxPtr = &p->maxValue;
			const char* format = GetFloatFormat(p);
			bool edited = false;
			if (p->editHint == CVarEditHint::Slider)
			{
				edited = ImGui::SliderScalar("", ImGuiDataType_Double, currentValue, minPtr, maxPtr, format);
			}
			else if (p->editHint == CVarEditHint::Drag)
			{
				edited = ImGui::DragScalar("", ImGuiDataType_Double, currentValue, p->step, minPtr, maxPtr, format);
			}
			else if (p->editHint == CVarEditHint::TextBox)
			{
				edited = ImGui::InputDouble("", currentValue, kZeroStep, kZeroStep, format);
			}
			if (edited)
			{
				*currentValue = ClampFloatValue(p, *currentValue);
			}
			ImGui::PopID();
		}
		break;

		case CVarType::STRING:

		if (readonlyFlag)
		{
			std::string displayFormat = p->name + "= %s";
			ImGui::PushID(p->name.c_str());
			ImGui::Text(displayFormat.c_str(), GetCVarArray<std::string>()->GetCurrent(p->arrayIndex).c_str());

			ImGui::PopID();
		}
		else
		{
			if (p->editHint == CVarEditHint::TextBox)
			{
				Label(p->name.c_str(), textWidth);
				std::vector<char> editBuffer(std::max<uint32_t>(1, p->stringMaxLength) + 1, '\0');
				std::string currentValue = GetCVarArray<std::string>()->GetCurrent(p->arrayIndex);
				std::strncpy(editBuffer.data(), currentValue.c_str(), editBuffer.size() - 1);
				editBuffer[editBuffer.size() - 1] = '\0';

				ImGui::PushID(p->name.c_str());
				if (ImGui::InputText("", editBuffer.data(), editBuffer.size()))
				{
					GetCVarArray<std::string>()->SetCurrent(ClampStringValue(p, editBuffer.data()), p->arrayIndex);
				}
				ImGui::PopID();
			}
		}
		break;

	default:
		break;
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("%s", p->description.c_str());
	}
}