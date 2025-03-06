#include "AIAgent.hpp"
#include <any>

std::any convertToType(const std::string& value, const std::string& type) {
	static const std::unordered_map<std::string, std::function<std::any(const std::string&)>> typeConverters = {
		{
			"std::string", [](const std::string& val) -> std::any { 
				return val; 
			}
		},
		{
			"int", [](const std::string& val) -> std::any {
				try { return std::stoi(val); }
				catch (...) { return std::any(); }
			}
		},
		{
			"float", [](const std::string& val) -> std::any {
				try { return std::stof(val); }
				catch (...) { return std::any(); }
			}
		},
		{
			"double", [](const std::string& val) -> std::any {
				try { return std::stod(val); }
				catch (...) { return std::any(); }
			}
		},
		{
			"bool", [](const std::string& val) -> std::any {
				return (val == "true" || val == "1") ? true : false;
			}
		}
	};

	auto it = typeConverters.find(type);
	if (it != typeConverters.end())
		return it->second(value);

	return std::any();
}

AIAgent::funcArg::funcArg(const std::string& inName, const std::string& inType, const std::string& inDesc, bool inRequired) :
	name(inName),
	type(inType),
	desc(inDesc),
	required(inRequired) {
}

AIAgent::tool* AIAgent::addTool(const std::string& name) {

	const auto& foundTool = this->tools.find(name);
	if (foundTool != this->tools.end())
		return foundTool->second;

	tool* newTool = new tool;
	this->tools[name] = newTool;

	return newTool;
}

void AIAgent::convertTools() {

	this->basePrompt.clear();

	this->basePrompt["tools"] = nlohmann::json::array();
	for (const auto& tool : this->tools) {
		
		std::vector<std::string> requiredArgs;
		nlohmann::json propertiesEntry;
		for (const auto& arg : tool.second->fn.args) {
			propertiesEntry[arg.name] = nlohmann::json::object();
			propertiesEntry[arg.name]["type"] = arg.type;
			propertiesEntry[arg.name]["description"] = arg.desc;

			if (arg.required)
				requiredArgs.emplace_back(arg.name);
		}

		const nlohmann::json& parametersEntry = {
			{"type", "object"},
			{"properties", propertiesEntry}
		};

		const nlohmann::json& functionEntry = {
			{"name", tool.first},
			{"description", tool.second->desc},
			{"parameters", parametersEntry},
			{"required", requiredArgs}
		};
		
		const nlohmann::json& toolEntry = {
			{"type", "function"},
			{"function", functionEntry}
		};

		this->basePrompt["tools"].emplace_back(toolEntry);
	}

	this->basePrompt["messages"] = nlohmann::json::array();	
	this->curPrompt = this->basePrompt;
}

size_t curlWriteString(char* contents, size_t size, size_t memSize, std::string* userPointer) {
	userPointer->append(contents, size * memSize);
	return size * memSize;
}
std::string AIAgent::queryLLM(const std::string& userPrompt) {

	if (this->curl == nullptr)
		this->curl = curl_easy_init();

	// Add base message
	this->curPrompt["messages"].clear();
	this->curPrompt["messages"].push_back(
		{
			{"role", "user"},
			{"content", userPrompt}
		}
	);

	return this->internalLLMQuery();
}

std::string AIAgent::internalLLMQuery() {

	const std::string& payLoad = this->curPrompt.dump(4);
	std::string retVal;

	struct curl_slist* headers = nullptr;
	headers = curl_slist_append(headers, "content-type: application/json");

	curl_easy_setopt(this->curl, CURLOPT_URL, this->llmUrl.c_str());
	curl_easy_setopt(this->curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(this->curl, CURLOPT_POST, 1L);
	curl_easy_setopt(this->curl, CURLOPT_POSTFIELDS, payLoad.c_str());
	curl_easy_setopt(this->curl, CURLOPT_WRITEFUNCTION, curlWriteString);
	curl_easy_setopt(this->curl, CURLOPT_WRITEDATA, &retVal);

	CURLcode res = curl_easy_perform(this->curl);
	if (res != CURLE_OK) {

		nlohmann::json error = {
			{"status", "error"},
			{"message", std::format("[!] AIAgent::internalLLMQuery 'curl_easy_perform' failed @ {} {} -> {}", __LINE__, __FILE__, curl_easy_strerror(res))}
		};

		return error.dump();
	}

	const nlohmann::json& jsonRet = nlohmann::json::parse(retVal);

	if (!jsonRet.contains("choices") || !jsonRet["choices"].is_array() || jsonRet["choices"].empty()) {
		
		nlohmann::json error = {
			{"status", "error"},
			{"message", std::format("[!] AIAgent::internalLLMQuery '{}' @ {} {}", R"(!jsonRet.contains("choices"))", __LINE__, __FILE__)}
		};

		return error.dump();
	}

	const nlohmann::json& firstChoice = jsonRet["choices"][0];

	if (!firstChoice.contains("message") || !firstChoice["message"].contains("tool_calls") || !firstChoice["message"]["tool_calls"].is_array()) {
		
		// Check if there is a finish_reason
		if (!firstChoice.contains("finish_reason")) {

			nlohmann::json error = {
				{"status", "error"},
				{"message", std::format("[!] AIAgent::internalLLMQuery '{}' @ {} {}", R"(!firstChoice.contains("finish_reason"))", __LINE__, __FILE__)}
			};

			return error.dump();
		}

		const std::string& finishReason = firstChoice["finish_reason"].get<std::string>();
		if (finishReason != "stop") {

			nlohmann::json error = {
				{"status", "error"},
				{"message", std::format("[!] AIAgent::internalLLMQuery '{}' @ {} {}", R"(finishReason != "stop")", __LINE__, __FILE__)}
			};

			return error.dump();
		}

		if (!firstChoice["message"].contains("content")) {

			nlohmann::json error = {
				{"status", "error"},
				{"message", std::format("[!] AIAgent::internalLLMQuery '{}' @ {} {}", R"(!firstChoice["message"].contains("content"))", __LINE__, __FILE__)}
			};

			return error.dump();

		}

		nlohmann::json success = {
			{"status", "success"},
			{"message", firstChoice["message"]["content"].get<std::string>()}
		};

		return success.dump();
	}

	const nlohmann::json& toolCalls = firstChoice["message"]["tool_calls"];

	for (const auto& toolCall : toolCalls) {
		const std::string& id = toolCall.value("id", "unknown");
		const std::string& type = toolCall.value("type", "unknown");

		if (!toolCall.contains("function") || !toolCall["function"].is_object())
			continue;

		const std::string& functionName = toolCall["function"].value("name", "unknown");
		const std::string& arguments = toolCall["function"].value("arguments", "{}");

		// Find the function
		const auto& foundFunc = this->tools.find(functionName);
		if (foundFunc == this->tools.end()) {
			
			nlohmann::json error = {
				{"status", "error"},
				{"message", std::format("[!] AIAgent::internalLLMQuery '{}' @ {} {}", R"(foundFunc == this->tools.end())", __LINE__, __FILE__)}
			};

			return error.dump();
		}

		// Parse arguments
		const nlohmann::json& jsonArgs = nlohmann::json::parse(arguments);

		// Combine the value with the value name in order
		AIFuncArg argList;
		for (const auto& arg : foundFunc->second->fn.args) {
			const std::string& argValStr = jsonArgs[arg.name].get<std::string>();
			argList._setArg(arg.name, convertToType(argValStr, arg.type));
		}

		// Call the function
		const std::optional<std::string>& funcRet = foundFunc->second->fn.func(&argList);
		if (!funcRet.has_value()) {

			nlohmann::json error = {
				{"status", "error"},
				{"message", std::format("[!] AIAgent::internalLLMQuery '{}' @ {} {}", R"(!funcRet.has_value())", __LINE__, __FILE__)}
			};

			return error.dump();
		}

		// Put the function return value as a message back into the LLM
		// Should construct a result message
		this->curPrompt["messages"].push_back(
			{
				{"role", "tool"},
				{"tool_call_id", id},
				{"content", funcRet.value()}
			}
		);
	}

	return this->internalLLMQuery();

}