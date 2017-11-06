//
//  main.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/11/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include <iostream>
#include <memory>
#include <cstdio>

#include <SDL2/SDL.h>

#include "../../StaticAnalyser/StaticAnalyser.hpp"
#include "../../Machines/Utility/MachineForTarget.hpp"

#include "../../Machines/ConfigurationTarget.hpp"
#include "../../Machines/CRTMachine.hpp"

#include "../../Concurrency/BestEffortUpdater.hpp"

namespace {

struct CRTMachineDelegate: public CRTMachine::Machine::Delegate {

	void machine_did_change_clock_rate(CRTMachine::Machine *machine) {
		best_effort_updater->set_clock_rate(machine->get_clock_rate());
	}

	void machine_did_change_clock_is_unlimited(CRTMachine::Machine *machine) {
	}

	Concurrency::BestEffortUpdater *best_effort_updater;
};

struct BestEffortUpdaterDelegate: public Concurrency::BestEffortUpdater::Delegate {

	void update(Concurrency::BestEffortUpdater *updater, int cycles, bool did_skip_previous_update) {
		machine->crt_machine()->run_for(Cycles(cycles));
	}

	Machine::DynamicMachine *machine;
};

}

int main(int argc, char *argv[]) {
	SDL_Window *window = nullptr;

	// Perform a sanity check on arguments.
	if(argc < 2) {
		std::cerr << "Usage: " << argv[0] << " [file]" << std::endl;
		return -1;
	}

	// Determine the machine for the supplied file.
	std::list<StaticAnalyser::Target> targets = StaticAnalyser::GetTargets(argv[1]);
	if(targets.empty()) {
		std::cerr << "Cannot open " << argv[1] << std::endl;
		return -1;
	}

	Concurrency::BestEffortUpdater updater;
	BestEffortUpdaterDelegate best_effort_updater_delegate;
	CRTMachineDelegate crt_delegate;

	// Create and configure a machine.
	std::unique_ptr<::Machine::DynamicMachine> machine(::Machine::MachineForTarget(targets.front()));

	updater.set_clock_rate(machine->crt_machine()->get_clock_rate());
	crt_delegate.best_effort_updater = &updater;
	best_effort_updater_delegate.machine = machine.get();

	machine->crt_machine()->set_delegate(&crt_delegate);
	updater.set_delegate(&best_effort_updater_delegate);

	// Attempt to set up video and audio.
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
		std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
		return -1;
	}

	// Ask for no depth buffer, a core profile and vsync-aligned rendering.
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetSwapInterval(1);

	window = SDL_CreateWindow(	"Clock Signal",
								SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
								400, 300,
								SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

	if(!window)
	{
		std::cerr << "Could not create window" << std::endl;
		return -1;
	}

	SDL_GLContext gl_context = SDL_GL_CreateContext(window);
	SDL_GL_MakeCurrent(window, gl_context);
	
	// Create an audio pipe.
//	SDL_AudioSpec desired_audio_spec;
//	SDL_AudioSpec obtained_audio_spec;
//	desired_audio_spec.freq = 192000;	// An arbitrary high number.
//	desired_audio_spec.format = AUDIO_S16;
//	desired_audio_spec.channels = 1;

	// Setup output, assuming a CRT machine for now, and prepare a best-effort updater.
	machine->crt_machine()->setup_output(4.0 / 3.0);
	machine->crt_machine()->get_crt()->set_output_gamma(2.2f);

	// For vanilla SDL purposes, assume system ROMs can be found in one of:
	//
	//	/usr/local/share/CLK/[system]; or
	//	/usr/share/CLK/[system]
	machine->crt_machine()->install_roms( [] (const std::string &machine, const std::string &name) -> std::unique_ptr<std::vector<uint8_t>> {
 		std::string local_path = "/usr/local/share/CLK/" + machine + "/" + name;
		FILE *file = fopen(local_path.c_str(), "r");
		if(!file) {
			std::string path = "/usr/share/CLK/" + machine + "/" + name;
			file = fopen(path.c_str(), "r");
		}

		if(!file) return nullptr;
		
		std::unique_ptr<std::vector<uint8_t>> data(new std::vector<uint8_t>);

		fseek(file, 0, SEEK_END);
		data->resize(ftell(file));
		fseek(file, 0, SEEK_SET);
		fread(data->data(), 1, data->size(), file);
		fclose(file);

		return data;
	});

	machine->configuration_target()->configure_as_target(targets.front());

	// For now, lie about audio output intentions.
	auto speaker = machine->crt_machine()->get_speaker();
	if(speaker) {
		speaker->set_output_rate(44100, 1024);
	}

	// Run the main event loop until the OS tells us to quit.
	bool should_quit = false;
	while(!should_quit) {
		// Process all pending events.
		SDL_Event event;
		while(SDL_PollEvent(&event)) {
			switch(event.type) {
//				default: std::cout << "Unhandled " << event.type << std::endl; break;
				case SDL_QUIT:	should_quit = true;	break;

				case SDL_KEYDOWN:
				break;
				case SDL_KEYUP:
				break;
			}
		}

		// Display a new frame and wait for vsync.
		updater.update();
		int width, height;
		SDL_GetWindowSize(window, &width, &height);
		machine->crt_machine()->get_crt()->draw_frame(static_cast<unsigned int>(width), static_cast<unsigned int>(height), false);
		SDL_GL_SwapWindow(window);
	}

	// Clean up.
	SDL_DestroyWindow( window );
	SDL_Quit();

	return 0;
}
