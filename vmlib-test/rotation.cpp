#include <catch2/catch_amalgamated.hpp>

#include <numbers>
#include <cmath>

#include "../vmlib/mat44.hpp"

TEST_CASE( "Rotation matrices", "[mat44]" )
{
	static constexpr float kEps_ = 1e-6f;

	using namespace Catch::Matchers;

	SECTION( "Rotation X - 90 degrees" )
	{
		float angle = 90.f * std::numbers::pi_v<float> / 180.f;
		auto const rot = make_rotation_x( angle );
		
		// Rotation around X axis should keep X component unchanged
		Vec4f v = { 1.f, 1.f, 0.f, 1.f };
		auto const result = rot * v;
		
		REQUIRE_THAT( result.x, WithinAbs( 1.f, kEps_ ) );
		REQUIRE_THAT( result.y, WithinAbs( 0.f, kEps_ ) );
		REQUIRE_THAT( result.z, WithinAbs( 1.f, kEps_ ) );
		REQUIRE_THAT( result.w, WithinAbs( 1.f, kEps_ ) );
	}

	SECTION( "Rotation Y - 90 degrees" )
	{
		float angle = 90.f * std::numbers::pi_v<float> / 180.f;
		auto const rot = make_rotation_y( angle );
		
		// Rotation around Y axis should keep Y component unchanged
		Vec4f v = { 1.f, 1.f, 0.f, 1.f };
		auto const result = rot * v;
		
		REQUIRE_THAT( result.x, WithinAbs( 0.f, kEps_ ) );
		REQUIRE_THAT( result.y, WithinAbs( 1.f, kEps_ ) );
		REQUIRE_THAT( result.z, WithinAbs( -1.f, kEps_ ) );
		REQUIRE_THAT( result.w, WithinAbs( 1.f, kEps_ ) );
	}

	SECTION( "Rotation Z - 90 degrees" )
	{
		float angle = 90.f * std::numbers::pi_v<float> / 180.f;
		auto const rot = make_rotation_z( angle );
		
		// Rotation around Z axis should keep Z component unchanged
		Vec4f v = { 1.f, 0.f, 1.f, 1.f };
		auto const result = rot * v;
		
		REQUIRE_THAT( result.x, WithinAbs( 0.f, kEps_ ) );
		REQUIRE_THAT( result.y, WithinAbs( 1.f, kEps_ ) );
		REQUIRE_THAT( result.z, WithinAbs( 1.f, kEps_ ) );
		REQUIRE_THAT( result.w, WithinAbs( 1.f, kEps_ ) );
	}

	SECTION( "Rotation X - 0 degrees is identity" )
	{
		auto const rot = make_rotation_x( 0.f );
		
		for( std::size_t i = 0; i < 4; ++i )
		{
			for( std::size_t j = 0; j < 4; ++j )
			{
				if( i == j )
					REQUIRE_THAT( rot.v[i*4 + j], WithinAbs( 1.f, kEps_ ) );
				else
					REQUIRE_THAT( rot.v[i*4 + j], WithinAbs( 0.f, kEps_ ) );
			}
		}
	}

	SECTION( "Rotation Y - 0 degrees is identity" )
	{
		auto const rot = make_rotation_y( 0.f );
		
		for( std::size_t i = 0; i < 4; ++i )
		{
			for( std::size_t j = 0; j < 4; ++j )
			{
				if( i == j )
					REQUIRE_THAT( rot.v[i*4 + j], WithinAbs( 1.f, kEps_ ) );
				else
					REQUIRE_THAT( rot.v[i*4 + j], WithinAbs( 0.f, kEps_ ) );
			}
		}
	}

	SECTION( "Rotation Z - 0 degrees is identity" )
	{
		auto const rot = make_rotation_z( 0.f );
		
		for( std::size_t i = 0; i < 4; ++i )
		{
			for( std::size_t j = 0; j < 4; ++j )
			{
				if( i == j )
					REQUIRE_THAT( rot.v[i*4 + j], WithinAbs( 1.f, kEps_ ) );
				else
					REQUIRE_THAT( rot.v[i*4 + j], WithinAbs( 0.f, kEps_ ) );
			}
		}
	}
}
