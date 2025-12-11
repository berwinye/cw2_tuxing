#include <catch2/catch_amalgamated.hpp>

#include <numbers>

#include "../vmlib/mat44.hpp"

TEST_CASE( "Matrix multiplication", "[mat44]" )
{
	static constexpr float kEps_ = 1e-6f;

	using namespace Catch::Matchers;

	SECTION( "Identity * Identity = Identity" )
	{
		auto const result = kIdentity44f * kIdentity44f;
		
		for( std::size_t i = 0; i < 4; ++i )
		{
			for( std::size_t j = 0; j < 4; ++j )
			{
				if( i == j )
					REQUIRE_THAT( result.v[i*4 + j], WithinAbs( 1.f, kEps_ ) );
				else
					REQUIRE_THAT( result.v[i*4 + j], WithinAbs( 0.f, kEps_ ) );
			}
		}
	}

	SECTION( "Matrix * Vector multiplication" )
	{
		// Test with identity matrix
		Vec4f v = { 1.f, 2.f, 3.f, 4.f };
		auto const result = kIdentity44f * v;
		
		REQUIRE_THAT( result.x, WithinAbs( 1.f, kEps_ ) );
		REQUIRE_THAT( result.y, WithinAbs( 2.f, kEps_ ) );
		REQUIRE_THAT( result.z, WithinAbs( 3.f, kEps_ ) );
		REQUIRE_THAT( result.w, WithinAbs( 4.f, kEps_ ) );
	}

	SECTION( "Translation matrix" )
	{
		auto const trans = make_translation( { 1.f, 2.f, 3.f } );
		Vec4f v = { 1.f, 1.f, 1.f, 1.f };
		auto const result = trans * v;
		
		REQUIRE_THAT( result.x, WithinAbs( 2.f, kEps_ ) );
		REQUIRE_THAT( result.y, WithinAbs( 3.f, kEps_ ) );
		REQUIRE_THAT( result.z, WithinAbs( 4.f, kEps_ ) );
		REQUIRE_THAT( result.w, WithinAbs( 1.f, kEps_ ) );
	}

	SECTION( "Scaling matrix" )
	{
		auto const scale = make_scaling( 2.f, 3.f, 4.f );
		Vec4f v = { 1.f, 1.f, 1.f, 1.f };
		auto const result = scale * v;
		
		REQUIRE_THAT( result.x, WithinAbs( 2.f, kEps_ ) );
		REQUIRE_THAT( result.y, WithinAbs( 3.f, kEps_ ) );
		REQUIRE_THAT( result.z, WithinAbs( 4.f, kEps_ ) );
		REQUIRE_THAT( result.w, WithinAbs( 1.f, kEps_ ) );
	}
}
