#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

GLuint space_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > space_meshes(LoadTagDefault, []() -> MeshBuffer const* {
	MeshBuffer const* ret = new MeshBuffer(data_path("space.pnct"));
	space_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
	});

Load< Scene > space_scene(LoadTagDefault, []() -> Scene const* {
	return new Scene(data_path("space.scene"), [&](Scene& scene, Scene::Transform* transform, std::string const& mesh_name) {
		Mesh const& mesh = space_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable& drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = space_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;
		});
	});



PlayMode::PlayMode() : scene(*space_scene) {//scene(*hexapod_scene) {

	for (auto& transform : scene.transforms) {
		if (transform.name == "Ship") { ship = &transform; }
		else if (transform.name.substr(0, 8) == "Asteroid") { asteroids.emplace_back(&transform); }
	}
	if (ship == nullptr) throw std::runtime_error("Ship not found.");
	if (asteroids.empty()) throw std::runtime_error("Not All Asteroids not found.");
	

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_a) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.downs += 1;
			down.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_q) {
			q.downs += 1;
			q.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_e) {
			e.downs += 1;
			e.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_SPACE) {
			space.downs += 1;
			space.pressed = true;
			return true;
		}


	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_q) {
			q.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_e) {
			e.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_SPACE) {
			space.pressed = false;
			return true;
		}
	} else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			return true;
		}
	} else if (evt.type == SDL_MOUSEMOTION) {
		if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
			glm::vec2 motion = glm::vec2(
				evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);
			camera->transform->rotation = glm::normalize(
				camera->transform->rotation
				* glm::angleAxis(-motion.x * camera->fovy, glm::vec3(0.0f, 1.0f, 0.0f))
				* glm::angleAxis(motion.y * camera->fovy, glm::vec3(1.0f, 0.0f, 0.0f))
			);
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {

	//move camera:
	{

		//combine inputs into a move:
		constexpr float PlayerSpeed = 20.0f;
		constexpr float Magic = .5f;
		constexpr float rMag = 8.0f;
		glm::vec3 cam_move = glm::vec3(0.0f);
		glm::vec3 rot = glm::vec3(0.0f);
		glm::vec3 ship_move = glm::vec3(0.0f);
		if (left.pressed && !right.pressed) cam_move.x =-1.0f;
		if (!left.pressed && right.pressed) cam_move.x = 1.0f;
		if (down.pressed && !up.pressed) cam_move.y =-1.0f;
		if (!down.pressed && up.pressed) cam_move.y = 1.0f;
		if (e.pressed) {
			ship_move.x = -1.0f;
			rot.z = 1.0f;
		}
		if (q.pressed) {
			ship_move.x = 1.0f;
			rot.z = -1.0f;
		}
		if (space.pressed) {
			ship->position = glm::vec3(.0f, .0f, 8.0f);
			ship->rotation = glm::angleAxis(.0f, glm::vec3(.0f));
			lose = false;
		}

		ship_move.y = -1.0f;


		//make it so that moving diagonally doesn't go faster:
		if (cam_move != glm::vec3(0.0f)) cam_move = glm::normalize(cam_move) * PlayerSpeed * elapsed;
		if (ship_move != glm::vec3(0.0f)) ship_move = glm::normalize(ship_move) * PlayerSpeed * elapsed;

		glm::mat4x3 ship_frame = ship->make_local_to_world();
		glm::vec3 ship_right = ship_frame[0];
		glm::vec3 ship_up = ship_frame[1];
		glm::vec3 ship_forward = -ship_frame[2];
		ship->position += ship_move.x * ship_right + ship_move.y * ship_forward + ship_move.z * ship_up;
		
		//math is hard
		float hpi = 1.570796f;
		float x_theta = glm::acos(ship->position.y / glm::length(glm::vec2(ship->position.y, ship->position.z)));
		if (ship->position.z < 0) {
			x_theta = std::abs(x_theta - 3.1415926f) + 3.1415926f;
		}
		float y_theta = glm::acos(ship->position.x / glm::length(glm::vec2(ship->position.x, ship->position.z))) - hpi;
		
		float z_theta = glm::acos(ship->position.y / glm::length(glm::vec2(ship->position.x, ship->position.y)));
		if (ship->position.x < 0) {
			z_theta = std::abs(z_theta - 3.1415926f) + 3.1415926f;
		}
		

		//Following math based on https://www.gamedev.net/forums/topic/625169-how-to-use-a-quaternion-as-a-camera-orientation/
		glm::quat x_quat = glm::angleAxis(x_theta, glm::vec3(1.0f, .0f, .0f));
		glm::quat y_quat = glm::angleAxis(y_theta, glm::vec3(.0f, 1.0f, .0f));
		glm::quat z_quat = glm::angleAxis(rot.z * elapsed * PlayerSpeed, glm::vec3(.0f, .0f, 1.0f));
	
		ship->rotation = x_quat * y_quat * z_quat;

		ship->position = glm::normalize(ship->position) * rMag;

		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 right = frame[0];
		glm::vec3 up = frame[1];
		glm::vec3 forward = -frame[2];

		camera->transform->position = ship->position + 5.0f * ship_forward + 7.0f * ship_up;
		camera->transform->rotation = x_quat * y_quat * glm::angleAxis(2.94f, glm::vec3(-1.0f, .0f, .0f)) * glm::angleAxis(3.14f, glm::vec3(.0f, .0f, 1.0f));
		//Uncomment below and comment above to have exterior camera view
		//camera->transform->position += cam_move.x * right + cam_move.y * forward;// +cam_move.z * up;
		
	}

	//Asteroids
	{
		static std::mt19937 mt;
		constexpr float AstSpeed = 25.0f;
		for (Scene::Transform* ast : asteroids) {

			if (glm::distance(ast->position, glm::vec3(0.0f)) < 5.0f) {
				//spawns randomly between 50 and 100 units away
				float x_val = (mt() / (float)(mt.max())) - .5f;
				float y_val = (mt() / (float)(mt.max())) - .5f;
				float z_val = (mt() / (float)(mt.max())) - .5f;
				ast->position = glm::normalize(glm::vec3(x_val, y_val, z_val)) * ((mt() / (float)(mt.max())) * 50.0f + 50.0f);
			}
			else {

				glm::vec3 ast_move = -1.0f * ast->position;

				if (ast_move != glm::vec3(0.0f)) ast_move = glm::normalize(ast_move) * AstSpeed * elapsed;

				ast->position.x += ast_move.x;
				ast->position.y += ast_move.y;
				ast->position.z += ast_move.z;
			}

			if (glm::distance(ast->position, ship->position) < 1.5f) {
				lose = true;
			}
		}
	}


	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
	q.downs = 0;
	e.downs = 0;
	space.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	GL_ERRORS();
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	GL_ERRORS();
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	GL_ERRORS();
	glUseProgram(0);

	glClearColor(.1f, 0.1f, 0.2f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		if (lose) {
			constexpr float H = 0.9f;
			lines.draw_text("     YOU LOSE",
				glm::vec3(-aspect + 0.25f * H, -1.0 + 0.5f * H, 0.0),
				glm::vec3(.5f * H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0x00, 0x00, 0x00, 0x00));
			float ofs = 2.0f / drawable_size.y;
			lines.draw_text("     YOU LOSE",
				glm::vec3(-aspect + 0.25f * H + ofs, -1.0 + +0.5f * H + ofs, 0.0),
				glm::vec3(.5f * H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		}
	}
}
