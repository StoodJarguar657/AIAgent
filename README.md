# AIAgent - C++ AI Agent Framework

AIAgent is a lightweight and flexible C++ framework for creating AI agents that can utilize various tools and functions to assist users. The framework is designed to be simple to use and extend, enabling developers to easily create custom AI agents capable of querying external services, interacting with user input, and calling functions dynamically.

This readme will guide you through the basic functionality of the agent and explain how you can extend the agent by adding tools and functions to interact with real-world services or APIs.

## Features

- **Simple Tool Integration**: Add tools to the agent by defining a function and describing its parameters.
- **Dynamic Function Calls**: Enable the agent to invoke functions dynamically based on input and parameters.
- **Easy to Extend**: Easily add new functionalities to your AI agent by expanding the toolset with custom logic.
- **Simple Interface**: Minimal setup for getting started with AI agents and integrating them into your C++ projects.

## Basic Example: Weather Query

Below is an example of a simple weather querying agent that uses a placeholder response. In the future, the `getWeather` function could be extended to make an actual API request to retrieve real weather data.

### Example Code:

```cpp
#include <iostream>

#include "AIAgent/AIAgent.hpp"

int main() {

	// Create agent
	AIAgent agent;

	// Create a tool
	AIAgent::tool* tool = agent.addTool("getWeater");
	tool->desc = "This gets the current weather in the given country.";
	
	// Describe all parameters with name, type, description and isRequired
	tool->fn.args.emplace_back("country", "std::string", "The country where the weather should be retrieved in.", true);
	
	// Define the callable function to the LLM
	tool->fn.func = [](AIAgent::AIFuncArg* arg) -> std::optional<std::string> {
		
		// This is how to retrieve a parameter from the LLM
		const auto& optCountry = arg->getVal<std::string>("country");
		if (optCountry.has_value()) {
			const std::string& country = optCountry.value();
			printf("getWeather arg(country): %s\n", country.c_str());
		}
		
		// If there is an error with a argument simply,
		// return "argument (...) is invalid"; or
		// return std::nullopt; This will cause the AIAgent to abort and exit

		// Placeholder response
		return R"({"location":{"name":"Berlin","region":"Berlin","country":"Germany","lat":52.517,"lon":13.4,"tz_id":"Europe/Berlin","localtime_epoch":1741257486,"localtime":"2025-03-06 11:38"},"current":{"last_updated_epoch":1741257000,"last_updated":"2025-03-06 11:30","temp_c":13.2,"temp_f":55.8,"is_day":1,"condition":{"text":"Sunny","icon":"//cdn.weatherapi.com/weather/64x64/day/113.png","code":1000},"wind_mph":6.0,"wind_kph":9.7,"wind_degree":229,"wind_dir":"SW","pressure_mb":1021.0,"pressure_in":30.15,"precip_mm":0.0,"precip_in":0.0,"humidity":41,"cloud":0,"feelslike_c":12.5,"feelslike_f":54.5,"windchill_c":10.5,"windchill_f":50.9,"heatindex_c":11.5,"heatindex_f":52.8,"dewpoint_c":0.2,"dewpoint_f":32.4,"vis_km":10.0,"vis_miles":6.0,"uv":2.3,"gust_mph":7.5,"gust_kph":12.0}})";
	};

	// Convert the tools into the OpenAI spec
	// https://cookbook.openai.com/examples/function_calling_with_an_openapi_spec
	agent.convertTools();

	// Prompt the LLM with a question
	// This will be a JSON structure
	// If "status" == "error", "message" contains the error
	// Otherwise "message" contains the response from the LLM
	std::string response = agent.queryLLM("What is the weather like in Germany?");

	return 0;
}
