#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <print>
#include <numbers>
#include <typeinfo>
#include <stdexcept>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <limits>
#include <map>

#include <cstdlib>

#include "../support/error.hpp"
#include "../support/program.hpp"
#include "../support/checkpoint.hpp"
#include "../support/debug_output.hpp"

#include "../vmlib/vec2.hpp"
#include "../vmlib/vec3.hpp"
#include "../vmlib/vec4.hpp"
#include "../vmlib/mat44.hpp"

#include <rapidobj/rapidobj.hpp>

#include <stb_image.h>

#include "defaults.hpp"


namespace
{
	constexpr char const* kWindowTitle = "COMP3811 - CW2";
	
	// Camera state
	struct Camera
	{
		Vec3f position{ 0.f, 10.f, 20.f };  // Start higher and further back
		float yaw = 0.f;      // Horizontal rotation (around Y axis)
		float pitch = -0.3f;    // Look slightly down
		
		Vec3f forward() const
		{
			float const cy = std::cos(yaw);
			float const sy = std::sin(yaw);
			float const cp = std::cos(pitch);
			float const sp = std::sin(pitch);
			return Vec3f{ cy * cp, sp, -sy * cp };
		}
		
		Vec3f right() const
		{
			float const cy = std::cos(yaw);
			float const sy = std::sin(yaw);
			return Vec3f{ cy, 0.f, -sy };
		}
		
		Vec3f up() const
		{
			return Vec3f{ 0.f, 1.f, 0.f };
		}
	};
	
	// Input state
	struct InputState
	{
		bool keys[512] = {};
		bool mouseActive = false;
		double lastMouseX = 0.0;
		double lastMouseY = 0.0;
		bool firstMouse = true;
	};
	
	// Launchpad mesh data (per material)
	struct LaunchpadMesh
	{
		GLuint vao = 0;
		GLuint vbo = 0;
		GLuint ebo = 0;
		std::size_t indexCount = 0;
		Vec3f materialColor{ 1.f, 1.f, 1.f };
	};
	
	// Space vehicle mesh data
	struct VehicleMesh
	{
		GLuint vao = 0;
		GLuint vbo = 0;
		GLuint ebo = 0;
		std::size_t indexCount = 0;
		Vec3f materialColor{ 0.8f, 0.2f, 0.2f }; // Red color
	};
	
	// Application state
	struct AppState
	{
		Camera camera;
		InputState input;
		float moveSpeed = 10.f;  // units per second
		float baseSpeed = 10.f;
		float speedMultiplier = 1.f;
		float mouseSensitivity = 0.002f;
		
		// Terrain mesh
		GLuint vao = 0;
		GLuint vbo = 0;
		GLuint ebo = 0;
		std::size_t indexCount = 0;
		
		GLuint texture = 0;
		bool hasTexture = false;
		
		// Launchpad meshes (one per material)
		std::vector<LaunchpadMesh> launchpadMeshes;
		
		// Space vehicle mesh
		VehicleMesh vehicleMesh;
		Vec3f vehiclePosition{ 0.f, 0.f, 0.f }; // Position on launchpad
		int vehicleLaunchpadIndex = 1; // Which launchpad (1 or 2)
		
		ShaderProgram* shaderProgram = nullptr;
		
		float fov = 60.f * std::numbers::pi_v<float> / 180.f;
		float nearPlane = 0.1f;
		float farPlane = 10000.f;  // Increased for large terrain models
		
		// Launchpad instance positions
		Vec3f launchpadPos1{ 0.f, 0.f, 0.f };
		Vec3f launchpadPos2{ 0.f, 0.f, 0.f };
	};
	
	AppState* gAppState = nullptr;
	
	void glfw_callback_error_( int, char const* );
	void glfw_callback_key_( GLFWwindow*, int, int, int, int );
	void glfw_callback_mouse_button_( GLFWwindow*, int, int, int );
	void glfw_callback_cursor_pos_( GLFWwindow*, double, double );
	void glfw_callback_framebuffer_size_( GLFWwindow*, int, int );

	struct GLFWCleanupHelper
	{
		~GLFWCleanupHelper();
	};
	struct GLFWWindowDeleter
	{
		~GLFWWindowDeleter();
		GLFWwindow* window;
	};

}

int main() try
{
	// Initialize GLFW
	if( GLFW_TRUE != glfwInit() )
	{
		char const* msg = nullptr;
		int ecode = glfwGetError( &msg );
		throw Error( "glfwInit() failed with '{}' ({})", msg, ecode );
	}

	// Ensure that we call glfwTerminate() at the end of the program.
	GLFWCleanupHelper cleanupHelper;

	// Configure GLFW and create window
	glfwSetErrorCallback( &glfw_callback_error_ );

	glfwWindowHint( GLFW_SRGB_CAPABLE, GLFW_TRUE );
	glfwWindowHint( GLFW_DOUBLEBUFFER, GLFW_TRUE );
	glfwWindowHint( GLFW_RESIZABLE, GLFW_TRUE );

	glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 4 );
	glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 3 );
	glfwWindowHint( GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE );
	glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );

	glfwWindowHint( GLFW_DEPTH_BITS, 24 );

#	if !defined(NDEBUG)
	// When building in debug mode, request an OpenGL debug context. This
	// enables additional debugging features. However, this can carry extra
	// overheads. We therefore do not do this for release builds.
	glfwWindowHint( GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE );
#	endif // ~ !NDEBUG

	GLFWwindow* window = glfwCreateWindow(
		1280,
		720,
		kWindowTitle,
		nullptr, nullptr
	);

	if( !window )
	{
		char const* msg = nullptr;
		int ecode = glfwGetError( &msg );
		throw Error( "glfwCreateWindow() failed with '{}' ({})", msg, ecode );
	}

	GLFWWindowDeleter windowDeleter{ window };


	// Set up event handling
	AppState appState;
	gAppState = &appState;
	
	glfwSetKeyCallback( window, &glfw_callback_key_ );
	glfwSetMouseButtonCallback( window, &glfw_callback_mouse_button_ );
	glfwSetCursorPosCallback( window, &glfw_callback_cursor_pos_ );
	glfwSetFramebufferSizeCallback( window, &glfw_callback_framebuffer_size_ );

	// Set up drawing stuff
	glfwMakeContextCurrent( window );
	glfwSwapInterval( 1 ); // V-Sync is on.

	// Initialize GLAD
	// This will load the OpenGL API. We mustn't make any OpenGL calls before this!
	if( !gladLoadGLLoader( (GLADloadproc)&glfwGetProcAddress ) )
		throw Error( "gladLoadGLLoader() failed - cannot load GL API!" );

	std::print( "RENDERER {}\n", (char const*)glGetString( GL_RENDERER ) );
	std::print( "VENDOR {}\n", (char const*)glGetString( GL_VENDOR ) );
	std::print( "VERSION {}\n", (char const*)glGetString( GL_VERSION ) );
	std::print( "SHADING_LANGUAGE_VERSION {}\n", (char const*)glGetString( GL_SHADING_LANGUAGE_VERSION ) );
	
	// Note: To use a different GPU, configure it in Windows Graphics Settings:
	// Settings > System > Display > Graphics Settings > Add your .exe > Options > High Performance

	// Ddebug output
#	if !defined(NDEBUG)
	setup_gl_debug_output();
#	endif // ~ !NDEBUG

	// Global GL state
	OGL_CHECKPOINT_ALWAYS();
	
	glEnable( GL_DEPTH_TEST );
	glEnable( GL_CULL_FACE );
	glCullFace( GL_BACK );
	glClearColor( 0.2f, 0.3f, 0.4f, 1.0f );

	OGL_CHECKPOINT_ALWAYS();

	// Get actual framebuffer size.
	// This can be different from the window size, as standard window
	// decorations (title bar, borders, ...) may be included in the window size
	// but not be part of the drawable surface area.
	int iwidth, iheight;
	glfwGetFramebufferSize( window, &iwidth, &iheight );

	glViewport( 0, 0, iwidth, iheight );

	// Other initialization & loading
	OGL_CHECKPOINT_ALWAYS();
	
	// Load shader
	ShaderProgram shader( {
		{ GL_VERTEX_SHADER, "assets/cw2/default.vert" },
		{ GL_FRAGMENT_SHADER, "assets/cw2/default.frag" }
	} );
	appState.shaderProgram = &shader;
	
	// Load OBJ file - use parlahti.obj
	std::filesystem::path objPath = "assets/cw2/parlahti.obj";
	if( !std::filesystem::exists( objPath ) )
	{
		objPath = "assets/cw2/langerso.obj";
		std::print( "parlahti.obj not found, using langerso.obj instead\n" );
	}
	auto result = rapidobj::ParseFile( objPath );
	
	if( result.error )
	{
		throw Error( "Failed to load OBJ file: {}", result.error.code.message() );
	}
	
	if( !rapidobj::Triangulate( result ) )
	{
		throw Error( "Failed to triangulate mesh" );
	}
	
	// Extract vertex data
	std::vector<float> vertices;
	std::vector<unsigned int> indices;
	
	bool hasTexCoords = !result.attributes.texcoords.empty();
	
	for( auto const& shape : result.shapes )
	{
		for( auto const& index : shape.mesh.indices )
		{
			// Position
			auto const& pos = result.attributes.positions;
			vertices.push_back( pos[3 * index.position_index + 0] );
			vertices.push_back( pos[3 * index.position_index + 1] );
			vertices.push_back( pos[3 * index.position_index + 2] );
			
			// Normal
			if( index.normal_index >= 0 )
			{
				auto const& norm = result.attributes.normals;
				vertices.push_back( norm[3 * index.normal_index + 0] );
				vertices.push_back( norm[3 * index.normal_index + 1] );
				vertices.push_back( norm[3 * index.normal_index + 2] );
			}
			else
			{
				vertices.push_back( 0.f );
				vertices.push_back( 0.f );
				vertices.push_back( 1.f );
			}
			
			// Texture coordinates
			if( hasTexCoords && index.texcoord_index >= 0 )
			{
				auto const& tex = result.attributes.texcoords;
				vertices.push_back( tex[2 * index.texcoord_index + 0] );
				vertices.push_back( tex[2 * index.texcoord_index + 1] );
			}
			else
			{
				vertices.push_back( 0.f );
				vertices.push_back( 0.f );
			}
			
			indices.push_back( static_cast<unsigned int>(indices.size()) );
		}
	}
	
	appState.indexCount = indices.size();
	
	// Debug output
	std::print( "Loaded {} vertices, {} indices\n", vertices.size() / 8, appState.indexCount );
	std::print( "Has texture coordinates: {}\n", hasTexCoords );
	if( appState.indexCount == 0 )
	{
		throw Error( "No indices loaded from OBJ file!" );
	}
	
	// Calculate model bounds for camera positioning
	float minX = std::numeric_limits<float>::max();
	float maxX = std::numeric_limits<float>::lowest();
	float minY = std::numeric_limits<float>::max();
	float maxY = std::numeric_limits<float>::lowest();
	float minZ = std::numeric_limits<float>::max();
	float maxZ = std::numeric_limits<float>::lowest();
	
	for( size_t i = 0; i < vertices.size(); i += 8 )
	{
		float x = vertices[i];
		float y = vertices[i + 1];
		float z = vertices[i + 2];
		minX = std::min( minX, x );
		maxX = std::max( maxX, x );
		minY = std::min( minY, y );
		maxY = std::max( maxY, y );
		minZ = std::min( minZ, z );
		maxZ = std::max( maxZ, z );
	}
	
	float centerX = (minX + maxX) * 0.5f;
	float centerY = (minY + maxY) * 0.5f;
	float centerZ = (minZ + maxZ) * 0.5f;
	float sizeX = maxX - minX;
	float sizeY = maxY - minY;
	float sizeZ = maxZ - minZ;
	float maxSize = std::max( { sizeX, sizeY, sizeZ } );
	
	std::print( "Model bounds: X[{}, {}] Y[{}, {}] Z[{}, {}]\n", minX, maxX, minY, maxY, minZ, maxZ );
	std::print( "Model center: ({}, {}, {})\n", centerX, centerY, centerZ );
	std::print( "Model size: ({}, {}, {})\n", sizeX, sizeY, sizeZ );
	
	// Position camera to view the model
	appState.camera.position = Vec3f{ centerX, centerY + maxSize * 0.5f, centerZ + maxSize * 1.5f };
	
	// Create VAO, VBO, EBO
	glGenVertexArrays( 1, &appState.vao );
	glGenBuffers( 1, &appState.vbo );
	glGenBuffers( 1, &appState.ebo );
	
	glBindVertexArray( appState.vao );
	
	glBindBuffer( GL_ARRAY_BUFFER, appState.vbo );
	glBufferData( GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW );
	
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, appState.ebo );
	glBufferData( GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW );
	
	// Position attribute (location 0)
	glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0 );
	glEnableVertexAttribArray( 0 );
	
	// Normal attribute (location 1)
	glVertexAttribPointer( 1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)) );
	glEnableVertexAttribArray( 1 );
	
	// Texture coordinate attribute (location 2)
	glVertexAttribPointer( 2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)) );
	glEnableVertexAttribArray( 2 );
	
	glBindVertexArray( 0 );
	
	OGL_CHECKPOINT_ALWAYS();
	
	// Load texture if available
	if( hasTexCoords && !result.materials.empty() )
	{
		// Try to find texture file from materials
		std::string texturePath;
		for( auto const& material : result.materials )
		{
			if( !material.diffuse_texname.empty() )
			{
				// Texture path relative to OBJ file
				std::filesystem::path texFile = material.diffuse_texname;
				std::filesystem::path objDir = objPath.parent_path();
				texturePath = (objDir / texFile).string();
				break;
			}
		}
		
		// If no texture found in materials, try common names
		if( texturePath.empty() )
		{
			std::filesystem::path objDir = objPath.parent_path();
			std::vector<std::string> commonNames = { "L4343A-4k.jpeg", "langerso.jpg", "langerso.png" };
			for( auto const& name : commonNames )
			{
				std::filesystem::path testPath = objDir / name;
				if( std::filesystem::exists( testPath ) )
				{
					texturePath = testPath.string();
					break;
				}
			}
		}
		
		if( !texturePath.empty() && std::filesystem::exists( texturePath ) )
		{
			std::print( "Loading texture: {}\n", texturePath );
			
			int width, height, channels;
			stbi_set_flip_vertically_on_load( true ); // OpenGL expects bottom-left origin
			unsigned char* data = stbi_load( texturePath.c_str(), &width, &height, &channels, 0 );
			
			if( data )
			{
				glGenTextures( 1, &appState.texture );
				glBindTexture( GL_TEXTURE_2D, appState.texture );
				
				// Set texture parameters
				glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
				glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
				glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
				glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
				
				// Determine format
				GLenum format = GL_RGB;
				if( channels == 1 )
					format = GL_RED;
				else if( channels == 3 )
					format = GL_RGB;
				else if( channels == 4 )
					format = GL_RGBA;
				
				// Upload texture data
				glTexImage2D( GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data );
				glGenerateMipmap( GL_TEXTURE_2D );
				
				stbi_image_free( data );
				appState.hasTexture = true;
				
				std::print( "Texture loaded successfully: {}x{} ({} channels)\n", width, height, channels );
			}
			else
			{
				std::print( stderr, "Failed to load texture: {}\n", stbi_failure_reason() );
			}
		}
		else
		{
			std::print( "No texture file found, rendering without texture\n" );
		}
	}
	
	OGL_CHECKPOINT_ALWAYS();
	
	// Load launchpad model for instancing
	std::filesystem::path launchpadPath = "assets/cw2/landingpad.obj";
	if( std::filesystem::exists( launchpadPath ) )
	{
		std::print( "Loading launchpad model: {}\n", launchpadPath.string() );
		auto launchpadResult = rapidobj::ParseFile( launchpadPath );
		
		if( launchpadResult.error )
		{
			std::print( stderr, "Warning: Failed to load launchpad OBJ: {}\n", launchpadResult.error.code.message() );
		}
		else
		{
			if( !rapidobj::Triangulate( launchpadResult ) )
			{
				std::print( stderr, "Warning: Failed to triangulate launchpad mesh\n" );
			}
			else
			{
				// Create a map to group indices by material
				std::map<int, std::vector<std::pair<std::vector<float>, std::vector<unsigned int>>>> materialGroups;
				
				// Extract vertex data grouped by material
				for( auto const& shape : launchpadResult.shapes )
				{
					for( size_t faceIdx = 0; faceIdx < shape.mesh.num_face_vertices.size(); ++faceIdx )
					{
						int materialId = -1;
						if( faceIdx < shape.mesh.material_ids.size() )
							materialId = shape.mesh.material_ids[faceIdx];
						
						// Get indices for this face
						size_t indexOffset = 0;
						for( size_t i = 0; i < faceIdx; ++i )
							indexOffset += shape.mesh.num_face_vertices[i];
						
						size_t numVerts = shape.mesh.num_face_vertices[faceIdx];
						std::vector<float> faceVertices;
						std::vector<unsigned int> faceIndices;
						
						for( size_t v = 0; v < numVerts; ++v )
						{
							auto const& index = shape.mesh.indices[indexOffset + v];
							
							// Position
							auto const& pos = launchpadResult.attributes.positions;
							faceVertices.push_back( pos[3 * index.position_index + 0] );
							faceVertices.push_back( pos[3 * index.position_index + 1] );
							faceVertices.push_back( pos[3 * index.position_index + 2] );
							
							// Normal
							if( index.normal_index >= 0 )
							{
								auto const& norm = launchpadResult.attributes.normals;
								faceVertices.push_back( norm[3 * index.normal_index + 0] );
								faceVertices.push_back( norm[3 * index.normal_index + 1] );
								faceVertices.push_back( norm[3 * index.normal_index + 2] );
							}
							else
							{
								faceVertices.push_back( 0.f );
								faceVertices.push_back( 0.f );
								faceVertices.push_back( 1.f );
							}
							
							// Texture coordinates (always add, even if empty)
							if( index.texcoord_index >= 0 && !launchpadResult.attributes.texcoords.empty() )
							{
								auto const& tex = launchpadResult.attributes.texcoords;
								faceVertices.push_back( tex[2 * index.texcoord_index + 0] );
								faceVertices.push_back( tex[2 * index.texcoord_index + 1] );
							}
							else
							{
								faceVertices.push_back( 0.f );
								faceVertices.push_back( 0.f );
							}
							
							faceIndices.push_back( static_cast<unsigned int>(faceIndices.size()) );
						}
						
						materialGroups[materialId].push_back( { faceVertices, faceIndices } );
					}
				}
				
				// Create mesh for each material
				for( auto& [materialId, groups] : materialGroups )
				{
					// Combine all faces for this material
					std::vector<float> vertices;
					std::vector<unsigned int> indices;
					unsigned int indexOffset = 0;
					
					for( auto& [faceVerts, faceInds] : groups )
					{
						vertices.insert( vertices.end(), faceVerts.begin(), faceVerts.end() );
						for( auto idx : faceInds )
						{
							indices.push_back( indexOffset + idx );
						}
						indexOffset += static_cast<unsigned int>(faceInds.size());
					}
					
					// Get material color
					Vec3f materialColor{ 1.f, 1.f, 1.f };
					if( materialId >= 0 && materialId < static_cast<int>(launchpadResult.materials.size()) )
					{
						auto const& mat = launchpadResult.materials[materialId];
						materialColor = Vec3f{ mat.diffuse[0], mat.diffuse[1], mat.diffuse[2] };
					}
					
					// Create OpenGL buffers
					LaunchpadMesh mesh;
					mesh.materialColor = materialColor;
					mesh.indexCount = indices.size();
					
					glGenVertexArrays( 1, &mesh.vao );
					glGenBuffers( 1, &mesh.vbo );
					glGenBuffers( 1, &mesh.ebo );
					
					glBindVertexArray( mesh.vao );
					
					glBindBuffer( GL_ARRAY_BUFFER, mesh.vbo );
					glBufferData( GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW );
					
					glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, mesh.ebo );
					glBufferData( GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW );
					
					// Position attribute (location 0)
					glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0 );
					glEnableVertexAttribArray( 0 );
					
					// Normal attribute (location 1)
					glVertexAttribPointer( 1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)) );
					glEnableVertexAttribArray( 1 );
					
					// Texture coordinate attribute (location 2)
					glVertexAttribPointer( 2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)) );
					glEnableVertexAttribArray( 2 );
					
					glBindVertexArray( 0 );
					
					appState.launchpadMeshes.push_back( mesh );
					
					std::print( "Created launchpad mesh for material {}: {} vertices, {} indices, color ({}, {}, {})\n",
						materialId, vertices.size() / 8, mesh.indexCount, materialColor.x, materialColor.y, materialColor.z );
				}
				
				// Find suitable positions for launchpad instances
				// Requirements:
				// - Must be in the sea (water areas)
				// - Must touch water but not be fully submerged
				// - Cannot be at origin (0,0,0)
				// - Should not be immediately next to each other
				float waterLevel = 0.5f; // Approximate water level
				
				// Calculate positions that are:
				// 1. In water areas (away from islands, which are typically at higher Y)
				// 2. Not at origin
				// 3. Sufficiently separated
				float offset1X = -sizeX * 0.35f;
				float offset1Z = -sizeZ * 0.35f;
				float offset2X = sizeX * 0.3f;
				float offset2Z = sizeZ * 0.3f;
				
				// Ensure positions are not at origin
				if( std::abs(centerX + offset1X) < 1.f && std::abs(centerZ + offset1Z) < 1.f )
					offset1X += sizeX * 0.1f;
				if( std::abs(centerX + offset2X) < 1.f && std::abs(centerZ + offset2Z) < 1.f )
					offset2X += sizeX * 0.1f;
				
				// Position 1: In water area, not at origin
				appState.launchpadPos1 = Vec3f{ centerX + offset1X, waterLevel, centerZ + offset1Z };
				
				// Position 2: Different location, also in water, separated from position 1
				appState.launchpadPos2 = Vec3f{ centerX + offset2X, waterLevel, centerZ + offset2Z };
				
				// Ensure minimum separation distance
				Vec3f separation = appState.launchpadPos2 - appState.launchpadPos1;
				float dist = length( separation );
				float minSeparation = sizeX * 0.2f; // Minimum 20% of terrain size
				if( dist < minSeparation )
				{
					// Adjust position 2 to be further away
					Vec3f dir = normalize( separation );
					appState.launchpadPos2 = appState.launchpadPos1 + dir * minSeparation;
				}
				
				std::print( "Launchpad instance positions:\n" );
				std::print( "  Position 1: ({:.2f}, {:.2f}, {:.2f})\n", 
					appState.launchpadPos1.x, appState.launchpadPos1.y, appState.launchpadPos1.z );
				std::print( "  Position 2: ({:.2f}, {:.2f}, {:.2f})\n", 
					appState.launchpadPos2.x, appState.launchpadPos2.y, appState.launchpadPos2.z );
				std::print( "  Separation distance: {:.2f}\n", length( appState.launchpadPos2 - appState.launchpadPos1 ) );
			}
		}
	}
	else
	{
		std::print( "Launchpad model not found: {}\n", launchpadPath.string() );
	}
	
	OGL_CHECKPOINT_ALWAYS();
	
	// Generate procedural space vehicle
	// Requirements: at least 7 shapes, 3 different types, all transformations, relative placement
	{
		std::print( "Generating procedural space vehicle...\n" );
		
		// Helper function to add a vertex with normal and texcoord
		auto addVertex = []( std::vector<float>& vertices, Vec3f pos, Vec3f normal, Vec2f tex = Vec2f{0.f, 0.f} )
		{
			vertices.push_back( pos.x );
			vertices.push_back( pos.y );
			vertices.push_back( pos.z );
			vertices.push_back( normal.x );
			vertices.push_back( normal.y );
			vertices.push_back( normal.z );
			vertices.push_back( tex.x );
			vertices.push_back( tex.y );
		};
		
		// Helper function to create a box
		auto createBox = [&addVertex]( std::vector<float>& vertices, std::vector<unsigned int>& indices, 
			Vec3f center, Vec3f size, Vec3f color )
		{
			unsigned int baseIndex = static_cast<unsigned int>(vertices.size() / 8);
			float hx = size.x * 0.5f;
			float hy = size.y * 0.5f;
			float hz = size.z * 0.5f;
			
			// Front face
			addVertex( vertices, Vec3f{ center.x - hx, center.y - hy, center.z + hz }, Vec3f{ 0.f, 0.f, 1.f } );
			addVertex( vertices, Vec3f{ center.x + hx, center.y - hy, center.z + hz }, Vec3f{ 0.f, 0.f, 1.f } );
			addVertex( vertices, Vec3f{ center.x + hx, center.y + hy, center.z + hz }, Vec3f{ 0.f, 0.f, 1.f } );
			addVertex( vertices, Vec3f{ center.x - hx, center.y + hy, center.z + hz }, Vec3f{ 0.f, 0.f, 1.f } );
			indices.push_back( baseIndex + 0 ); indices.push_back( baseIndex + 1 ); indices.push_back( baseIndex + 2 );
			indices.push_back( baseIndex + 0 ); indices.push_back( baseIndex + 2 ); indices.push_back( baseIndex + 3 );
			baseIndex += 4;
			
			// Back face
			addVertex( vertices, Vec3f{ center.x + hx, center.y - hy, center.z - hz }, Vec3f{ 0.f, 0.f, -1.f } );
			addVertex( vertices, Vec3f{ center.x - hx, center.y - hy, center.z - hz }, Vec3f{ 0.f, 0.f, -1.f } );
			addVertex( vertices, Vec3f{ center.x - hx, center.y + hy, center.z - hz }, Vec3f{ 0.f, 0.f, -1.f } );
			addVertex( vertices, Vec3f{ center.x + hx, center.y + hy, center.z - hz }, Vec3f{ 0.f, 0.f, -1.f } );
			indices.push_back( baseIndex + 0 ); indices.push_back( baseIndex + 1 ); indices.push_back( baseIndex + 2 );
			indices.push_back( baseIndex + 0 ); indices.push_back( baseIndex + 2 ); indices.push_back( baseIndex + 3 );
			baseIndex += 4;
			
			// Top face
			addVertex( vertices, Vec3f{ center.x - hx, center.y + hy, center.z - hz }, Vec3f{ 0.f, 1.f, 0.f } );
			addVertex( vertices, Vec3f{ center.x - hx, center.y + hy, center.z + hz }, Vec3f{ 0.f, 1.f, 0.f } );
			addVertex( vertices, Vec3f{ center.x + hx, center.y + hy, center.z + hz }, Vec3f{ 0.f, 1.f, 0.f } );
			addVertex( vertices, Vec3f{ center.x + hx, center.y + hy, center.z - hz }, Vec3f{ 0.f, 1.f, 0.f } );
			indices.push_back( baseIndex + 0 ); indices.push_back( baseIndex + 1 ); indices.push_back( baseIndex + 2 );
			indices.push_back( baseIndex + 0 ); indices.push_back( baseIndex + 2 ); indices.push_back( baseIndex + 3 );
			baseIndex += 4;
			
			// Bottom face
			addVertex( vertices, Vec3f{ center.x - hx, center.y - hy, center.z + hz }, Vec3f{ 0.f, -1.f, 0.f } );
			addVertex( vertices, Vec3f{ center.x - hx, center.y - hy, center.z - hz }, Vec3f{ 0.f, -1.f, 0.f } );
			addVertex( vertices, Vec3f{ center.x + hx, center.y - hy, center.z - hz }, Vec3f{ 0.f, -1.f, 0.f } );
			addVertex( vertices, Vec3f{ center.x + hx, center.y - hy, center.z + hz }, Vec3f{ 0.f, -1.f, 0.f } );
			indices.push_back( baseIndex + 0 ); indices.push_back( baseIndex + 1 ); indices.push_back( baseIndex + 2 );
			indices.push_back( baseIndex + 0 ); indices.push_back( baseIndex + 2 ); indices.push_back( baseIndex + 3 );
			baseIndex += 4;
			
			// Right face
			addVertex( vertices, Vec3f{ center.x + hx, center.y - hy, center.z + hz }, Vec3f{ 1.f, 0.f, 0.f } );
			addVertex( vertices, Vec3f{ center.x + hx, center.y - hy, center.z - hz }, Vec3f{ 1.f, 0.f, 0.f } );
			addVertex( vertices, Vec3f{ center.x + hx, center.y + hy, center.z - hz }, Vec3f{ 1.f, 0.f, 0.f } );
			addVertex( vertices, Vec3f{ center.x + hx, center.y + hy, center.z + hz }, Vec3f{ 1.f, 0.f, 0.f } );
			indices.push_back( baseIndex + 0 ); indices.push_back( baseIndex + 1 ); indices.push_back( baseIndex + 2 );
			indices.push_back( baseIndex + 0 ); indices.push_back( baseIndex + 2 ); indices.push_back( baseIndex + 3 );
			baseIndex += 4;
			
			// Left face
			addVertex( vertices, Vec3f{ center.x - hx, center.y - hy, center.z - hz }, Vec3f{ -1.f, 0.f, 0.f } );
			addVertex( vertices, Vec3f{ center.x - hx, center.y - hy, center.z + hz }, Vec3f{ -1.f, 0.f, 0.f } );
			addVertex( vertices, Vec3f{ center.x - hx, center.y + hy, center.z + hz }, Vec3f{ -1.f, 0.f, 0.f } );
			addVertex( vertices, Vec3f{ center.x - hx, center.y + hy, center.z - hz }, Vec3f{ -1.f, 0.f, 0.f } );
			indices.push_back( baseIndex + 0 ); indices.push_back( baseIndex + 1 ); indices.push_back( baseIndex + 2 );
			indices.push_back( baseIndex + 0 ); indices.push_back( baseIndex + 2 ); indices.push_back( baseIndex + 3 );
		};
		
		// Helper function to create a cylinder
		auto createCylinder = [&addVertex]( std::vector<float>& vertices, std::vector<unsigned int>& indices,
			Vec3f center, float radius, float height, int segments, Vec3f color )
		{
			unsigned int baseIndex = static_cast<unsigned int>(vertices.size() / 8);
			float halfHeight = height * 0.5f;
			
			// Top and bottom circles
			for( int i = 0; i <= segments; ++i )
			{
				float angle = 2.f * std::numbers::pi_v<float> * i / segments;
				float x = std::cos( angle );
				float z = std::sin( angle );
				
				// Top vertex
				addVertex( vertices, Vec3f{ center.x + radius * x, center.y + halfHeight, center.z + radius * z }, Vec3f{ x, 0.f, z } );
				// Bottom vertex
				addVertex( vertices, Vec3f{ center.x + radius * x, center.y - halfHeight, center.z + radius * z }, Vec3f{ x, 0.f, z } );
			}
			
			// Side faces
			for( int i = 0; i < segments; ++i )
			{
				unsigned int top1 = baseIndex + i * 2;
				unsigned int top2 = baseIndex + (i + 1) * 2;
				unsigned int bot1 = baseIndex + i * 2 + 1;
				unsigned int bot2 = baseIndex + (i + 1) * 2 + 1;
				
				indices.push_back( top1 ); indices.push_back( bot1 ); indices.push_back( top2 );
				indices.push_back( bot1 ); indices.push_back( bot2 ); indices.push_back( top2 );
			}
			
			baseIndex += (segments + 1) * 2;
			
			// Top cap
			for( int i = 0; i < segments; ++i )
			{
				float angle1 = 2.f * std::numbers::pi_v<float> * i / segments;
				float angle2 = 2.f * std::numbers::pi_v<float> * (i + 1) / segments;
				float x1 = std::cos( angle1 );
				float z1 = std::sin( angle1 );
				float x2 = std::cos( angle2 );
				float z2 = std::sin( angle2 );
				
				addVertex( vertices, Vec3f{ center.x, center.y + halfHeight, center.z }, Vec3f{ 0.f, 1.f, 0.f } );
				addVertex( vertices, Vec3f{ center.x + radius * x1, center.y + halfHeight, center.z + radius * z1 }, Vec3f{ 0.f, 1.f, 0.f } );
				addVertex( vertices, Vec3f{ center.x + radius * x2, center.y + halfHeight, center.z + radius * z2 }, Vec3f{ 0.f, 1.f, 0.f } );
				indices.push_back( baseIndex ); indices.push_back( baseIndex + 1 ); indices.push_back( baseIndex + 2 );
				baseIndex += 3;
			}
			
			// Bottom cap
			for( int i = 0; i < segments; ++i )
			{
				float angle1 = 2.f * std::numbers::pi_v<float> * i / segments;
				float angle2 = 2.f * std::numbers::pi_v<float> * (i + 1) / segments;
				float x1 = std::cos( angle1 );
				float z1 = std::sin( angle1 );
				float x2 = std::cos( angle2 );
				float z2 = std::sin( angle2 );
				
				addVertex( vertices, Vec3f{ center.x, center.y - halfHeight, center.z }, Vec3f{ 0.f, -1.f, 0.f } );
				addVertex( vertices, Vec3f{ center.x + radius * x2, center.y - halfHeight, center.z + radius * z2 }, Vec3f{ 0.f, -1.f, 0.f } );
				addVertex( vertices, Vec3f{ center.x + radius * x1, center.y - halfHeight, center.z + radius * z1 }, Vec3f{ 0.f, -1.f, 0.f } );
				indices.push_back( baseIndex ); indices.push_back( baseIndex + 1 ); indices.push_back( baseIndex + 2 );
				baseIndex += 3;
			}
		};
		
		// Helper function to create a sphere
		auto createSphere = [&addVertex]( std::vector<float>& vertices, std::vector<unsigned int>& indices,
			Vec3f center, float radius, int segments, Vec3f color )
		{
			unsigned int baseIndex = static_cast<unsigned int>(vertices.size() / 8);
			
			// Generate vertices
			for( int i = 0; i <= segments; ++i )
			{
				float theta = std::numbers::pi_v<float> * i / segments; // 0 to PI
				float sinTheta = std::sin( theta );
				float cosTheta = std::cos( theta );
				
				for( int j = 0; j <= segments; ++j )
				{
					float phi = 2.f * std::numbers::pi_v<float> * j / segments; // 0 to 2*PI
					float sinPhi = std::sin( phi );
					float cosPhi = std::cos( phi );
					
					float x = sinTheta * cosPhi;
					float y = cosTheta;
					float z = sinTheta * sinPhi;
					
					Vec3f pos{ center.x + radius * x, center.y + radius * y, center.z + radius * z };
					Vec3f normal{ x, y, z };
					addVertex( vertices, pos, normal );
				}
			}
			
			// Generate indices
			for( int i = 0; i < segments; ++i )
			{
				for( int j = 0; j < segments; ++j )
				{
					int first = baseIndex + i * (segments + 1) + j;
					int second = first + segments + 1;
					
					indices.push_back( first );
					indices.push_back( second );
					indices.push_back( first + 1 );
					
					indices.push_back( second );
					indices.push_back( second + 1 );
					indices.push_back( first + 1 );
				}
			}
		};
		
		// Build space vehicle: Rocket design
		// Using at least 7 shapes and 3 different types (box, cylinder, sphere)
		std::vector<float> vehicleVertices;
		std::vector<unsigned int> vehicleIndices;
		
		// Base position (will be placed on launchpad)
		Vec3f vehicleBase{ 0.f, 0.f, 0.f };
		
		// 1. Main body (cylinder) - scaled
		createCylinder( vehicleVertices, vehicleIndices, 
			Vec3f{ vehicleBase.x, vehicleBase.y + 1.5f, vehicleBase.z }, 
			0.4f, 3.0f, 16, Vec3f{ 0.8f, 0.2f, 0.2f } );
		
		// 2. Nose cone (cylinder, scaled and rotated) - on top of main body
		createCylinder( vehicleVertices, vehicleIndices,
			Vec3f{ vehicleBase.x, vehicleBase.y + 3.8f, vehicleBase.z },
			0.35f, 0.8f, 16, Vec3f{ 0.9f, 0.3f, 0.3f } );
		
		// 3. Top sphere (sphere) - on top of nose cone
		createSphere( vehicleVertices, vehicleIndices,
			Vec3f{ vehicleBase.x, vehicleBase.y + 4.5f, vehicleBase.z },
			0.3f, 12, Vec3f{ 1.0f, 0.4f, 0.4f } );
		
		// 4. Left fin (box, scaled and rotated) - relative to main body
		createBox( vehicleVertices, vehicleIndices,
			Vec3f{ vehicleBase.x - 0.5f, vehicleBase.y + 0.8f, vehicleBase.z },
			Vec3f{ 0.1f, 0.6f, 0.4f }, Vec3f{ 0.7f, 0.1f, 0.1f } );
		
		// 5. Right fin (box, scaled) - relative to main body
		createBox( vehicleVertices, vehicleIndices,
			Vec3f{ vehicleBase.x + 0.5f, vehicleBase.y + 0.8f, vehicleBase.z },
			Vec3f{ 0.1f, 0.6f, 0.4f }, Vec3f{ 0.7f, 0.1f, 0.1f } );
		
		// 6. Front fin (box, scaled) - relative to main body
		createBox( vehicleVertices, vehicleIndices,
			Vec3f{ vehicleBase.x, vehicleBase.y + 0.8f, vehicleBase.z + 0.5f },
			Vec3f{ 0.4f, 0.6f, 0.1f }, Vec3f{ 0.7f, 0.1f, 0.1f } );
		
		// 7. Engine nozzle (cylinder, scaled) - at bottom of main body
		createCylinder( vehicleVertices, vehicleIndices,
			Vec3f{ vehicleBase.x, vehicleBase.y - 0.3f, vehicleBase.z },
			0.5f, 0.4f, 16, Vec3f{ 0.3f, 0.3f, 0.3f } );
		
		// 8. Window (small sphere, scaled) - on main body
		createSphere( vehicleVertices, vehicleIndices,
			Vec3f{ vehicleBase.x, vehicleBase.y + 2.2f, vehicleBase.z + 0.4f },
			0.15f, 8, Vec3f{ 0.2f, 0.5f, 0.9f } );
		
		// Create OpenGL buffers for vehicle
		appState.vehicleMesh.indexCount = vehicleIndices.size();
		appState.vehicleMesh.materialColor = Vec3f{ 0.8f, 0.2f, 0.2f };
		
		glGenVertexArrays( 1, &appState.vehicleMesh.vao );
		glGenBuffers( 1, &appState.vehicleMesh.vbo );
		glGenBuffers( 1, &appState.vehicleMesh.ebo );
		
		glBindVertexArray( appState.vehicleMesh.vao );
		
		glBindBuffer( GL_ARRAY_BUFFER, appState.vehicleMesh.vbo );
		glBufferData( GL_ARRAY_BUFFER, vehicleVertices.size() * sizeof(float), vehicleVertices.data(), GL_STATIC_DRAW );
		
		glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, appState.vehicleMesh.ebo );
		glBufferData( GL_ELEMENT_ARRAY_BUFFER, vehicleIndices.size() * sizeof(unsigned int), vehicleIndices.data(), GL_STATIC_DRAW );
		
		// Position attribute (location 0)
		glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0 );
		glEnableVertexAttribArray( 0 );
		
		// Normal attribute (location 1)
		glVertexAttribPointer( 1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)) );
		glEnableVertexAttribArray( 1 );
		
		// Texture coordinate attribute (location 2)
		glVertexAttribPointer( 2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)) );
		glEnableVertexAttribArray( 2 );
		
		glBindVertexArray( 0 );
		
		// Place vehicle on launchpad 1
		appState.vehicleLaunchpadIndex = 1;
		appState.vehiclePosition = appState.launchpadPos1;
		appState.vehiclePosition.y += 0.5f; // Place on top of launchpad
		
		std::print( "Space vehicle created: {} vertices, {} indices\n", vehicleVertices.size() / 8, appState.vehicleMesh.indexCount );
		std::print( "Vehicle placed on launchpad {} at ({:.2f}, {:.2f}, {:.2f})\n",
			appState.vehicleLaunchpadIndex, appState.vehiclePosition.x, appState.vehiclePosition.y, appState.vehiclePosition.z );
	}
	
	OGL_CHECKPOINT_ALWAYS();

	// Timing for frame-rate independent movement
	auto lastTime = Clock::now();
	
	// Main loop
	while( !glfwWindowShouldClose( window ) )
	{
		// Calculate delta time
		auto currentTime = Clock::now();
		Secondsf deltaTime = std::chrono::duration_cast<Secondsf>( currentTime - lastTime );
		lastTime = currentTime;
		float dt = deltaTime.count();
		
		// Let GLFW process events
		glfwPollEvents();
		
		// Check if window was resized.
		float fbwidth, fbheight;
		{
			int nwidth, nheight;
			glfwGetFramebufferSize( window, &nwidth, &nheight );

			fbwidth = float(nwidth);
			fbheight = float(nheight);

			if( 0 == nwidth || 0 == nheight )
			{
				// Window minimized? Pause until it is unminimized.
				// This is a bit of a hack.
				do
				{
					glfwWaitEvents();
					glfwGetFramebufferSize( window, &nwidth, &nheight );
				} while( 0 == nwidth || 0 == nheight );
			}

			glViewport( 0, 0, nwidth, nheight );
		}

		// Update state
		// Update speed multiplier
		appState.speedMultiplier = 1.f;
		if( appState.input.keys[GLFW_KEY_LEFT_SHIFT] || appState.input.keys[GLFW_KEY_RIGHT_SHIFT] )
			appState.speedMultiplier *= 5.f;
		if( appState.input.keys[GLFW_KEY_LEFT_CONTROL] || appState.input.keys[GLFW_KEY_RIGHT_CONTROL] )
			appState.speedMultiplier *= 0.2f;
		
		appState.moveSpeed = appState.baseSpeed * appState.speedMultiplier;
		
		// Update camera position
		Vec3f moveDir{ 0.f, 0.f, 0.f };
		if( appState.input.keys[GLFW_KEY_W] )
			moveDir = moveDir + appState.camera.forward();
		if( appState.input.keys[GLFW_KEY_S] )
			moveDir = moveDir - appState.camera.forward();
		if( appState.input.keys[GLFW_KEY_A] )
			moveDir = moveDir - appState.camera.right();
		if( appState.input.keys[GLFW_KEY_D] )
			moveDir = moveDir + appState.camera.right();
		if( appState.input.keys[GLFW_KEY_E] )
			moveDir = moveDir + appState.camera.up();
		if( appState.input.keys[GLFW_KEY_Q] )
			moveDir = moveDir - appState.camera.up();
		
		// Normalize and apply movement
		float moveLen = length( moveDir );
		if( moveLen > 0.001f )
		{
			moveDir = normalize( moveDir );
			appState.camera.position = appState.camera.position + moveDir * (appState.moveSpeed * dt);
		}

		// Draw scene
		OGL_CHECKPOINT_DEBUG();
		
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		
		// Calculate matrices
		float aspect = fbwidth / fbheight;
		Mat44f proj = make_perspective_projection( appState.fov, aspect, appState.nearPlane, appState.farPlane );
		
		// View matrix (camera) - look-at inverse
		Vec3f fwd = appState.camera.forward();
		Vec3f right = appState.camera.right();
		Vec3f up = appState.camera.up();
		Vec3f pos = appState.camera.position;
		
		// View matrix is the inverse of the camera transform
		// First row: right vector
		// Second row: up vector  
		// Third row: -forward vector
		// Fourth row: translation (dot product with negated position)
		Mat44f view{ {
			right.x, right.y, right.z, -(right.x * pos.x + right.y * pos.y + right.z * pos.z),
			up.x, up.y, up.z, -(up.x * pos.x + up.y * pos.y + up.z * pos.z),
			-fwd.x, -fwd.y, -fwd.z, (fwd.x * pos.x + fwd.y * pos.y + fwd.z * pos.z),
			0.f, 0.f, 0.f, 1.f
		} };
		
		// Model matrix (identity for now)
		Mat44f model = kIdentity44f;
		
		Mat44f mvp = proj * view * model;
		
		// Use shader
		glUseProgram( shader.programId() );
		
		// Set uniforms
		GLint mvpLoc = glGetUniformLocation( shader.programId(), "uModelViewProjection" );
		GLint modelLoc = glGetUniformLocation( shader.programId(), "uModel" );
		GLint lightDirLoc = glGetUniformLocation( shader.programId(), "uLightDir" );
		GLint ambientLoc = glGetUniformLocation( shader.programId(), "uAmbientColor" );
		GLint diffuseLoc = glGetUniformLocation( shader.programId(), "uDiffuseColor" );
		GLint useTextureLoc = glGetUniformLocation( shader.programId(), "uUseTexture" );
		
		if( mvpLoc == -1 || modelLoc == -1 || lightDirLoc == -1 || ambientLoc == -1 || diffuseLoc == -1 || useTextureLoc == -1 )
		{
			std::print( stderr, "Warning: Some uniform locations are invalid\n" );
		}
		
		// Transpose for OpenGL (column-major)
		Mat44f mvpT = transpose( mvp );
		Mat44f modelT = transpose( model );
		if( mvpLoc != -1 )
			glUniformMatrix4fv( mvpLoc, 1, GL_FALSE, mvpT.v );
		if( modelLoc != -1 )
			glUniformMatrix4fv( modelLoc, 1, GL_FALSE, modelT.v );
		
		// Light direction (0, 1, -1) normalized in world space
		// According to task requirements, use simplified directional light model
		// with only ambient and diffuse components
		Vec3f lightDir = normalize( Vec3f{ 0.f, 1.f, -1.f } );
		if( lightDirLoc != -1 )
			glUniform3f( lightDirLoc, lightDir.x, lightDir.y, lightDir.z );
		
		if( ambientLoc != -1 )
			glUniform3f( ambientLoc, 0.2f, 0.2f, 0.2f );
		if( diffuseLoc != -1 )
			glUniform3f( diffuseLoc, 0.8f, 0.8f, 0.8f );
		
		// Bind texture if available
		if( appState.hasTexture && appState.texture != 0 )
		{
			glActiveTexture( GL_TEXTURE0 );
			glBindTexture( GL_TEXTURE_2D, appState.texture );
			GLint textureLoc = glGetUniformLocation( shader.programId(), "uTexture" );
			if( textureLoc != -1 )
				glUniform1i( textureLoc, 0 ); // Bind to texture unit 0
			if( useTextureLoc != -1 )
				glUniform1i( useTextureLoc, 1 );
		}
		else
		{
			if( useTextureLoc != -1 )
				glUniform1i( useTextureLoc, 0 );
		}
		
		// Draw terrain mesh
		glBindVertexArray( appState.vao );
		glDrawElements( GL_TRIANGLES, static_cast<GLsizei>(appState.indexCount), GL_UNSIGNED_INT, 0 );
		glBindVertexArray( 0 );
		
		// Draw launchpad instances (instancing - same data, different positions)
		if( !appState.launchpadMeshes.empty() )
		{
			// Disable texture for launchpad (uses material colors)
			if( useTextureLoc != -1 )
				glUniform1i( useTextureLoc, 0 );
			
			GLint materialColorLoc = glGetUniformLocation( shader.programId(), "uMaterialColor" );
			GLint useMaterialColorLoc = glGetUniformLocation( shader.programId(), "uUseMaterialColor" );
			
			if( useMaterialColorLoc != -1 )
				glUniform1i( useMaterialColorLoc, 1 );
			
			// Render launchpad at position 1
			Mat44f launchpadModel1 = make_translation( appState.launchpadPos1 );
			Mat44f launchpadMvp1 = proj * view * launchpadModel1;
			Mat44f launchpadModel1T = transpose( launchpadModel1 );
			Mat44f launchpadMvp1T = transpose( launchpadMvp1 );
			
			if( mvpLoc != -1 )
				glUniformMatrix4fv( mvpLoc, 1, GL_FALSE, launchpadMvp1T.v );
			if( modelLoc != -1 )
				glUniformMatrix4fv( modelLoc, 1, GL_FALSE, launchpadModel1T.v );
			
			for( auto const& mesh : appState.launchpadMeshes )
			{
				if( materialColorLoc != -1 )
					glUniform3f( materialColorLoc, mesh.materialColor.x, mesh.materialColor.y, mesh.materialColor.z );
				
				glBindVertexArray( mesh.vao );
				glDrawElements( GL_TRIANGLES, static_cast<GLsizei>(mesh.indexCount), GL_UNSIGNED_INT, 0 );
				glBindVertexArray( 0 );
			}
			
			// Render launchpad at position 2
			Mat44f launchpadModel2 = make_translation( appState.launchpadPos2 );
			Mat44f launchpadMvp2 = proj * view * launchpadModel2;
			Mat44f launchpadModel2T = transpose( launchpadModel2 );
			Mat44f launchpadMvp2T = transpose( launchpadMvp2 );
			
			if( mvpLoc != -1 )
				glUniformMatrix4fv( mvpLoc, 1, GL_FALSE, launchpadMvp2T.v );
			if( modelLoc != -1 )
				glUniformMatrix4fv( modelLoc, 1, GL_FALSE, launchpadModel2T.v );
			
			for( auto const& mesh : appState.launchpadMeshes )
			{
				if( materialColorLoc != -1 )
					glUniform3f( materialColorLoc, mesh.materialColor.x, mesh.materialColor.y, mesh.materialColor.z );
				
				glBindVertexArray( mesh.vao );
				glDrawElements( GL_TRIANGLES, static_cast<GLsizei>(mesh.indexCount), GL_UNSIGNED_INT, 0 );
				glBindVertexArray( 0 );
			}
			
			// Re-enable texture for terrain if needed
			if( appState.hasTexture && appState.texture != 0 && useTextureLoc != -1 )
				glUniform1i( useTextureLoc, 1 );
			
			if( useMaterialColorLoc != -1 )
				glUniform1i( useMaterialColorLoc, 0 );
		}
		
		// Draw space vehicle
		if( appState.vehicleMesh.vao != 0 && appState.vehicleMesh.indexCount > 0 )
		{
			// Disable texture, use material color
			if( useTextureLoc != -1 )
				glUniform1i( useTextureLoc, 0 );
			
			GLint materialColorLoc = glGetUniformLocation( shader.programId(), "uMaterialColor" );
			GLint useMaterialColorLoc = glGetUniformLocation( shader.programId(), "uUseMaterialColor" );
			
			if( useMaterialColorLoc != -1 )
				glUniform1i( useMaterialColorLoc, 1 );
			
			// Create model matrix for vehicle (translation to launchpad position)
			Mat44f vehicleModel = make_translation( appState.vehiclePosition );
			Mat44f vehicleMvp = proj * view * vehicleModel;
			Mat44f vehicleModelT = transpose( vehicleModel );
			Mat44f vehicleMvpT = transpose( vehicleMvp );
			
			if( mvpLoc != -1 )
				glUniformMatrix4fv( mvpLoc, 1, GL_FALSE, vehicleMvpT.v );
			if( modelLoc != -1 )
				glUniformMatrix4fv( modelLoc, 1, GL_FALSE, vehicleModelT.v );
			
			if( materialColorLoc != -1 )
				glUniform3f( materialColorLoc, appState.vehicleMesh.materialColor.x, 
					appState.vehicleMesh.materialColor.y, appState.vehicleMesh.materialColor.z );
			
			glBindVertexArray( appState.vehicleMesh.vao );
			glDrawElements( GL_TRIANGLES, static_cast<GLsizei>(appState.vehicleMesh.indexCount), GL_UNSIGNED_INT, 0 );
			glBindVertexArray( 0 );
			
			if( useMaterialColorLoc != -1 )
				glUniform1i( useMaterialColorLoc, 0 );
		}

		OGL_CHECKPOINT_DEBUG();

		// Display results
		glfwSwapBuffers( window );
	}

	// Cleanup.
	if( appState.vao != 0 )
		glDeleteVertexArrays( 1, &appState.vao );
	if( appState.vbo != 0 )
		glDeleteBuffers( 1, &appState.vbo );
	if( appState.ebo != 0 )
		glDeleteBuffers( 1, &appState.ebo );
	if( appState.texture != 0 )
		glDeleteTextures( 1, &appState.texture );
	
	// Cleanup launchpad meshes
	for( auto& mesh : appState.launchpadMeshes )
	{
		if( mesh.vao != 0 )
			glDeleteVertexArrays( 1, &mesh.vao );
		if( mesh.vbo != 0 )
			glDeleteBuffers( 1, &mesh.vbo );
		if( mesh.ebo != 0 )
			glDeleteBuffers( 1, &mesh.ebo );
	}
	
	// Cleanup vehicle mesh
	if( appState.vehicleMesh.vao != 0 )
		glDeleteVertexArrays( 1, &appState.vehicleMesh.vao );
	if( appState.vehicleMesh.vbo != 0 )
		glDeleteBuffers( 1, &appState.vehicleMesh.vbo );
	if( appState.vehicleMesh.ebo != 0 )
		glDeleteBuffers( 1, &appState.vehicleMesh.ebo );
	
	return 0;
}
catch( std::exception const& eErr )
{
	std::print( stderr, "Top-level Exception ({}):\n", typeid(eErr).name() );
	std::print( stderr, "{}\n", eErr.what() );
	std::print( stderr, "Bye.\n" );
	return 1;
}


namespace
{
	void glfw_callback_error_( int aErrNum, char const* aErrDesc )
	{
		std::print( stderr, "GLFW error: {} ({})\n", aErrDesc, aErrNum );
	}

	void glfw_callback_key_( GLFWwindow* aWindow, int aKey, int, int aAction, int )
	{
		if( !gAppState )
			return;
			
		if( GLFW_KEY_ESCAPE == aKey && GLFW_PRESS == aAction )
		{
			glfwSetWindowShouldClose( aWindow, GLFW_TRUE );
			return;
		}
		
		// Update key state
		if( aAction == GLFW_PRESS )
			gAppState->input.keys[aKey] = true;
		else if( aAction == GLFW_RELEASE )
			gAppState->input.keys[aKey] = false;
	}
	
	void glfw_callback_mouse_button_( GLFWwindow* aWindow, int aButton, int aAction, int )
	{
		if( !gAppState )
			return;
			
		if( aButton == GLFW_MOUSE_BUTTON_RIGHT && aAction == GLFW_PRESS )
		{
			gAppState->input.mouseActive = !gAppState->input.mouseActive;
			
			if( gAppState->input.mouseActive )
			{
				glfwSetInputMode( aWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED );
				gAppState->input.firstMouse = true;
			}
			else
			{
				glfwSetInputMode( aWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL );
			}
		}
	}
	
	void glfw_callback_cursor_pos_( GLFWwindow* /*aWindow*/, double aX, double aY )
	{
		if( !gAppState || !gAppState->input.mouseActive )
			return;
			
		if( gAppState->input.firstMouse )
		{
			gAppState->input.lastMouseX = aX;
			gAppState->input.lastMouseY = aY;
			gAppState->input.firstMouse = false;
		}
		
		double xoffset = aX - gAppState->input.lastMouseX;
		double yoffset = gAppState->input.lastMouseY - aY;  // Reversed since y-coordinates go from bottom to top
		
		gAppState->input.lastMouseX = aX;
		gAppState->input.lastMouseY = aY;
		
		xoffset *= gAppState->mouseSensitivity;
		yoffset *= gAppState->mouseSensitivity;
		
		gAppState->camera.yaw += static_cast<float>(xoffset);
		gAppState->camera.pitch += static_cast<float>(yoffset);
		
		// Constrain pitch
		constexpr float maxPitch = std::numbers::pi_v<float> / 2.f - 0.1f;
		if( gAppState->camera.pitch > maxPitch )
			gAppState->camera.pitch = maxPitch;
		if( gAppState->camera.pitch < -maxPitch )
			gAppState->camera.pitch = -maxPitch;
	}
	
	void glfw_callback_framebuffer_size_( GLFWwindow*, int aWidth, int aHeight )
	{
		glViewport( 0, 0, aWidth, aHeight );
	}

}

namespace
{
	GLFWCleanupHelper::~GLFWCleanupHelper()
	{
		glfwTerminate();
	}

	GLFWWindowDeleter::~GLFWWindowDeleter()
	{
		if( window )
			glfwDestroyWindow( window );
	}
}
