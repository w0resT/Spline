#pragma once

class spline {
public:
	spline( );
	~spline( );

	void clear( ) {
		m_S.clear( );
		m_P.clear( );
		m_a.clear( );
		m_b.clear( );

		m_n = 0;
	}

	void update( const std::vector<vec2> &vec_P, int sp_type, int sp_param, bool loc_param, float step ) {
		// Empty vec :(
		if ( vec_P.empty( ) ) {
			return;
		}

		// Clearing last stuff
		clear( );

		m_spline_type = sp_type;
		m_parametrization = sp_param;
		m_local_param = loc_param;

		m_step = step;

		m_P = vec_P;			// Copy vec x
		m_n = m_P.size( ) - 1;	// Getting number of lines	

		update_stuff( );
	}

	void update_stuff( ) {
		// Fill a, b
		for ( size_t i = 1; i <= m_n; ++i ) {
			// Если включена локальная параметризация
			if ( m_local_param ) {
				m_a.push_back( 0.f );
			}
			else {
				m_a.push_back( m_P[ i ].x );
			}

			// Хордовая параметризация
			if ( m_parametrization == 0 ) {
				auto vec_p = m_P[ i ] - m_P[ i - 1 ];
				auto len_p = vec_p.length( );
				m_b.push_back( m_a.back() + len_p );

				m_step = 1.f;
			}
			// Нормализованная параметризация
			else {
				m_b.push_back( m_a.back() + 1.f );
			}
		}

		std::cout << "m_a.size(): " << m_a.size( ) << " | m_b.size(): " << m_b.size( ) << std::endl;

		calc( );
	}

	void calc( ) {
		std::vector< Eigen::Matrix<float, 4, 4>> A;
		std::vector< Eigen::Matrix<float, 4, 4>> B;
		std::vector< Eigen::Matrix<float, 4, 4>> U;

		Eigen::Matrix<float, 4, 4> C;
		Eigen::Matrix<float, 4, 4> D;

		// Calc A, B, U; i = [1..n]
		for ( int i = 0; i < m_n; ++i ) {
			// Matrix A
			Eigen::Matrix<float, 4, 4> Ai;
			Eigen::Matrix<float, 4, 4> Bi;
			

			// Calc Ai
			{
				// T(ai)
				{
					Ai( 0, 0 ) = 1;
					Ai( 1, 0 ) = m_a[ i ];
					Ai( 2, 0 ) = std::powf( m_a[ i ], 2.f );
					Ai( 3, 0 ) = std::powf( m_a[ i ], 3.f );
				}

				// T(bi)
				{
					Ai( 0, 1 ) = 1;
					Ai( 1, 1 ) = m_b[ i ];
					Ai( 2, 1 ) = std::powf( m_b[ i ], 2.f );
					Ai( 3, 1 ) = std::powf( m_b[ i ], 3.f );
				}

				// T'(bi)
				{
					Ai( 0, 2 ) = 0;
					Ai( 1, 2 ) = 1;
					Ai( 2, 2 ) = 2.f * m_b[ i ];
					Ai( 3, 2 ) = 3.f * std::powf( m_b[ i ], 2.f );
				}

				// T''(bi)
				{
					Ai( 0, 3 ) = 0;
					Ai( 1, 3 ) = 0;
					Ai( 2, 3 ) = 2;
					Ai( 3, 3 ) = 6.f * m_b[ i ];
				}

				std::cout << "matrix Ai = \n" << Ai << std::endl;
			}

			// Calc Bi
			{
				// 0
				{
					Bi( 0, 0 ) = 0;
					Bi( 1, 0 ) = 0;
					Bi( 2, 0 ) = 0;
					Bi( 3, 0 ) = 0;
				}

				// 0
				{
					Bi( 0, 1 ) = 0;
					Bi( 1, 1 ) = 0;
					Bi( 2, 1 ) = 0;
					Bi( 3, 1 ) = 0;
				}

				// T'(ai)
				{
					Bi( 0, 2 ) = 0;
					Bi( 1, 2 ) = 1;
					Bi( 2, 2 ) = 2.f * m_a[ i ];
					Bi( 3, 2 ) = 3.f * std::powf( m_a[ i ], 2.f );
				}

				// T''(ai)
				{
					Bi( 0, 3 ) = 0;
					Bi( 1, 3 ) = 0;
					Bi( 2, 3 ) = 2;
					Bi( 3, 3 ) = 6.f * m_a[ i ];
				}

				 std::cout << "matrix Bi = \n" << Bi << std::endl;
			}

			

			// Adding values
			A.push_back( Ai );
			B.push_back( Bi );
		}

		// Calc Ui
		for ( int i = 1; i <= m_n; ++i ) {
			Eigen::Matrix<float, 4, 4> Ui;

			// Pi-1
			{
				Ui( 0, 0 ) = m_P[ i - 1 ].x;
				Ui( 1, 0 ) = m_P[ i - 1 ].y;
				Ui( 2, 0 ) = 0;
				Ui( 3, 0 ) = 0;
			}

			// Pi
			{
				Ui( 0, 1 ) = m_P[ i ].x;
				Ui( 1, 1 ) = m_P[ i ].y;
				Ui( 2, 1 ) = 0;
				Ui( 3, 1 ) = 0;
			}

			// 0
			{
				Ui( 0, 2 ) = 0;
				Ui( 1, 2 ) = 0;
				Ui( 2, 2 ) = 0;
				Ui( 3, 2 ) = 0;
			}

			// 0
			{
				Ui( 0, 3 ) = 0;
				Ui( 1, 3 ) = 0;
				Ui( 2, 3 ) = 0;
				Ui( 3, 3 ) = 0;
			}

			std::cout << "matrix Ui = \n" << Ui << std::endl;

			U.push_back( Ui );
		}

		// Calc C
		C = A.back( );

		// Calc D
		// Cyclic spline
		if ( m_spline_type == 0 ) {
			D = B.front( );
		}
		// Acyclic spline
		else {
			D = -( B.front( ) );
		}
		
		// Recurrent algorithm for calculating the parameters of a cubic spline
		std::vector< Eigen::Matrix<float, 4, 4>> L;
		std::vector< Eigen::Matrix<float, 4, 4>> M;
		std::vector< Eigen::Matrix<float, 4, 4>> S;

		if ( m_n == 1 ) {
			auto s1 = U.back( ) * ( ( C - D ).inverse( ) );
			S.push_back( s1 );

		}
		else {
			// Step 1
			// Calc L, M; i = [n-1, 1]
			for ( int i = m_n - 1 - 1; i >= 0; --i ) {
				Eigen::Matrix<float, 4, 4> Li;
				Eigen::Matrix<float, 4, 4> Mi;;

				// Calc L
				{
					// Doing this only on first iteration
					if ( i == m_n - 1 - 1 ) {
						Li = ( U[ i ] ) * ( A[ i ].inverse( ) );
					}
					else {
						Li = ( U[ i ] + L.back( ) * B[ i + 1 ] ) * ( A[ i ].inverse( ) );
					}

					std::cout << "matrix Li = \n" << Li << std::endl;
				}

				// Calc M
				{
					// Doing this only on first iteration
					if ( i == m_n - 1 - 1 ) {
						Mi = ( B[ i + 1 ] ) * ( A[ i ].inverse( ) );
					}
					else {
						Mi = M.back( ) * B[ i + 1 ] * ( A[ i ].inverse( ) );
					}

					std::cout << "matrix Mi = \n" << Mi << std::endl;
				}

				L.push_back( Li );
				M.push_back( Mi );
			}

			// Step 2
			// Calc Sn, Si; i = [n-1, 1]
			for ( int i = m_n - 1 - 1; i >= 0; --i ) {
				Eigen::Matrix<float, 4, 4> Si;

				// Calc S
				{
					// Doing this only on first iteration
					if ( i == m_n - 1 - 1 ) {
						Si = ( U.back( ) + L.back( ) * D ) * ( ( C - M.back( ) * D ).inverse( ) );
						std::cout << "matrix Si = \n" << Si << std::endl;

						S.push_back( Si );

						Si = ( U[ i ] + S.back( ) * B[ i + 1 ] ) * ( A[ i ].inverse( ) );
					}
					else {
						Si = ( U[ i ] + S.back( ) * B[ i + 1 ] ) * ( A[ i ].inverse( ) );
					}

					std::cout << "matrix Si = \n" << Si << std::endl;
				}

				S.push_back( Si );
			}

			std::reverse( S.begin(), S.end() );
		}

		// [ai, bi]
		for ( size_t i = 0; i < m_a.size( ); ++i ) {
			vec2 si;

			// Step m_step = 1.f;
			for ( float t = m_a[ i ]; t <= m_b[ i ] + m_step; t += m_step ) {
				si.x = ( S[ i ]( 0, 0 ) ) 
					+ ( S[ i ]( 0, 1 ) * t ) 
					+ ( S[ i ]( 0, 2 ) * std::powf( t, 2.f ) ) 
					+ ( S[ i ]( 0, 3 ) * std::powf( t, 3.f ) );

				si.y = ( S[ i ]( 1, 0 ) )
					+ ( S[ i ]( 1, 1 ) * t )
					+ ( S[ i ]( 1, 2 ) * std::powf( t, 2.f ) )
					+ ( S[ i ]( 1, 3 ) * std::powf( t, 3.f ) );

				m_S.push_back( si );
			}		
		}

	}

	bool is_spline_exist( ) {
		return m_S.size( ) != 0;
	}

	auto get_splines( ) {
		return m_S;
	}

private:
	// S, i = [0..n]; Si = [si0, si1, si2, si3]
	std::vector<vec2> m_S;
	std::vector<vec2> m_P;

	std::vector<float> m_a;
	std::vector<float> m_b;

	// Count of lines
	int m_n;

	// 0 - Cyclic, 1 - Acyclic
	int m_spline_type;

	// 0 - Chordate, 1 - Normalized
	int m_parametrization = 0;

	// Is local parametrization enable?
	bool m_local_param;

	// Step for 't'
	float m_step = 0.1f;
};

spline::spline( ) {
	m_n = 0;
	m_spline_type = 0;
	m_local_param = false;
}

spline::~spline( ) {
}