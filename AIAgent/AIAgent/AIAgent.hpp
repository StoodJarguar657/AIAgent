#ifndef AIAGENT_HPP
#define AIAGENT_HPP

// External dependencies
#include <Json/json.hpp>
#include <Curl/Curl.h>

#include <unordered_map>
#include <optional>
#include <vector>
#include <string>

class AIAgent {
private:

	CURL* curl = nullptr;
	nlohmann::json basePrompt;
	nlohmann::json curPrompt;

public:

	struct AIFuncArg {
	private:
		std::unordered_map<std::string, std::any> args;
	public:
		
		void _setArg(const std::string& name, const std::any& value) {
			this->args.insert({ name, value });
		}

		template <typename T>
		std::optional<T> getVal(const std::string& name) {
			const auto& foundArg = this->args.find(name);
			if (foundArg == this->args.end())
				return std::nullopt;
			return std::any_cast<T>(foundArg->second);
		}
	};

	struct funcArg {
		std::string name = "";
		std::string type = "";
		std::string desc = "";
		bool required = false;
		funcArg(const std::string& inName, const std::string& inType, const std::string& inDesc, bool inRequired);
	};
	struct func {
		std::function<std::optional<std::string>(AIFuncArg*)> func;
		std::vector<funcArg> args = {};
	};
	struct tool {
		func fn;
		std::string desc;
	};
	tool* addTool(const std::string& name);

	void convertTools();

	std::string llmUrl = "http://localhost:1234/v1/chat/completions";
	std::string queryLLM(const std::string& userPrompt);

private:

	std::string internalLLMQuery();

	struct LLMResponse {

	};

	std::unordered_map<std::string, tool*> tools;

};

#endif AIAGENT_HPP