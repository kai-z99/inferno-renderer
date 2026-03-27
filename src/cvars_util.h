#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>

#include <glm/vec3.hpp>

class CVarParameter;

inline double ClampFloatValue(const CVarParameter* parameter, double value)
{
	return std::clamp(value, parameter->minValue, parameter->maxValue);
}

inline int32_t ClampIntValue(const CVarParameter* parameter, int32_t value)
{
	return std::clamp(value, parameter->intMinValue, parameter->intMaxValue);
}

inline const char* GetIntFormat(const CVarParameter* parameter)
{
	if (parameter->intFormat.empty())
	{
		return "%i";
	}
	return parameter->intFormat.c_str();
}

inline std::string ClampStringValue(const CVarParameter* parameter, const std::string& value)
{
	const size_t maxLength = static_cast<size_t>(std::max<uint32_t>(1, parameter->stringMaxLength));
	if (value.size() <= maxLength)
	{
		return value;
	}
	return value.substr(0, maxLength);
}

inline glm::vec3 ClampVec3Value(const CVarParameter* parameter, const glm::vec3& value)
{
	glm::vec3 clamped;
	clamped.x = std::clamp(value.x, parameter->vec3MinValue, parameter->vec3MaxValue);
	clamped.y = std::clamp(value.y, parameter->vec3MinValue, parameter->vec3MaxValue);
	clamped.z = std::clamp(value.z, parameter->vec3MinValue, parameter->vec3MaxValue);
	return clamped;
}

inline bool IsEditHintCompatible(CVarType type, CVarEditHint hint)
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

inline void ValidateEditHintCompatibility(const CVarParameter* parameter)
{
	if (!IsEditHintCompatible(parameter->type, parameter->editHint))
	{
		std::fprintf(stderr, "Invalid CVar edit hint for '%s'\n", parameter->name.c_str());
	}
	assert(IsEditHintCompatible(parameter->type, parameter->editHint) && "CVar edit hint is not compatible with its value type.");
}
