#include <catch2/catch_amalgamated.hpp>

#include <numbers>

#include "../vmlib/mat44.hpp"

TEST_CASE( "Translation and Scaling matrices", "[mat44]" )
{
	static constexpr float kEps_ = 1e-6f;

	using namespace Catch::Matchers;

	SECTION( "Translation matrix basic" )
	{
		auto const trans = make_translation( { 5.f, 10.f, 15.f } );
		
		// Check the translation components
		REQUIRE_THAT( trans.v[3], WithinAbs( 5.f, kEps_ ) );
		REQUIRE_THAT( trans.v[7], WithinAbs( 10.f, kEps_ ) );
		REQUIRE_THAT( trans.v[11], WithinAbs( 15.f, kEps_ ) );
		
		// Check that it's otherwise identity
		for( std::size_t i = 0; i < 3; ++i )
		{
			for( std::size_t j = 0; j < 3; ++j )
			{
				if( i == j )
					REQUIRE_THAT( trans.v[i*4 + j], WithinAbs( 1.f, kEps_ ) );
				else
					REQUIRE_THAT( trans.v[i*4 + j], WithinAbs( 0.f, kEps_ ) );
			}
		}
	}

	SECTION( "Translation applied to vector" )
	{
		auto const trans = make_translation( { 2.f, 3.f, 4.f } );
		Vec4f v = { 1.f, 1.f, 1.f, 1.f };
		auto const result = trans * v;
		
		REQUIRE_THAT( result.x, WithinAbs( 3.f, kEps_ ) );
		REQUIRE_THAT( result.y, WithinAbs( 4.f, kEps_ ) );
		REQUIRE_THAT( result.z, WithinAbs( 5.f, kEps_ ) );
		REQUIRE_THAT( result.w, WithinAbs( 1.f, kEps_ ) );
	}

	SECTION( "Scaling matrix basic" )
	{
		auto const scale = make_scaling( 2.f, 3.f, 4.f );
		
		// Check the scaling components
		REQUIRE_THAT( scale.v[0], WithinAbs( 2.f, kEps_ ) );
		REQUIRE_THAT( scale.v[5], WithinAbs( 3.f, kEps_ ) );
		REQUIRE_THAT( scale.v[10], WithinAbs( 4.f, kEps_ ) );
		REQUIRE_THAT( scale.v[15], WithinAbs( 1.f, kEps_ ) );
		
		// Check that off-diagonal elements are zero
		for( std::size_t i = 0; i < 4; ++i )
		{
			for( std::size_t j = 0; j < 4; ++j )
			{
				if( i != j )
					REQUIRE_THAT( scale.v[i*4 + j], WithinAbs( 0.f, kEps_ ) );
			}
		}
	}

	SECTION( "Scaling applied to vector" )
	{
		auto const scale = make_scaling( 2.f, 3.f, 4.f );
		Vec4f v = { 2.f, 2.f, 2.f, 1.f };
		auto const result = scale * v;
		
		REQUIRE_THAT( result.x, WithinAbs( 4.f, kEps_ ) );
		REQUIRE_THAT( result.y, WithinAbs( 6.f, kEps_ ) );
		REQUIRE_THAT( result.z, WithinAbs( 8.f, kEps_ ) );
		REQUIRE_THAT( result.w, WithinAbs( 1.f, kEps_ ) );
	}

	SECTION( "Translation composition" )
	{
		auto const trans1 = make_translation( { 1.f, 2.f, 3.f } );
		auto const trans2 = make_translation( { 4.f, 5.f, 6.f } );
		auto const combined = trans1 * trans2;
		
		// Composing translations should add them
		REQUIRE_THAT( combined.v[3], WithinAbs( 5.f, kEps_ ) );
		REQUIRE_THAT( combined.v[7], WithinAbs( 7.f, kEps_ ) );
		REQUIRE_THAT( combined.v[11], WithinAbs( 9.f, kEps_ ) );
	}

	SECTION( "Scaling composition" )
	{
		auto const scale1 = make_scaling( 2.f, 2.f, 2.f );
		auto const scale2 = make_scaling( 3.f, 3.f, 3.f );
		auto const combined = scale1 * scale2;
		
		// Composing scalings should multiply them
		REQUIRE_THAT( combined.v[0], WithinAbs( 6.f, kEps_ ) );
		REQUIRE_THAT( combined.v[5], WithinAbs( 6.f, kEps_ ) );
		REQUIRE_THAT( combined.v[10], WithinAbs( 6.f, kEps_ ) );
	}
}
