// https://github.com/joye-ramone/xray_xp_dev/commit/77d8bb876df84e69ee589232577fc1b7886b3663#diff-38bdb068bbc18c1790e04671a77c5407

#include <windows.h> // for GetAsyncKeyState
#include <cmath>
#include "MetroDeveloper.h"
#include "wpn_bobbing_la.h"

/* some X-Ray specific types */
float _cos(float a) { return cos(a); }
float _sin(float a) { return sin(a); }
float _abs(float a) { return fabs(a); }

typedef unsigned u32;
typedef unsigned long long u64;

struct Fvector
{
	float x, y, z;
};

struct Fvector4
{
	float x, y, z, w;
	
	void set(const Fvector4 &other);
};

void Fvector4::set(const Fvector4 &other)
{
	x = other.x;
	y = other.y;
	z = other.z;
	w = other.w;
}

struct Fmatrix
{
	union {
		struct {
			float _11, _12, _13, _14;
			float _21, _22, _23, _24;
			float _31, _32, _33, _34;
			float _41, _42, _43, _44;
		};
		struct {
			Fvector4 i;
			Fvector4 j;
			Fvector4 k;
			Fvector4 c;
		};
	};
	
	void mul(const Fmatrix &a, const Fmatrix &b);
	void setHPB(float h, float p, float b);
};

void Fmatrix::mul(const Fmatrix &a, const Fmatrix &b)
{
	_11 = a._11*b._11 + a._21*b._12 + a._31*b._13 + a._41*b._14;
	_12 = a._12*b._11 + a._22*b._12 + a._32*b._13 + a._42*b._14;
	_13 = a._13*b._11 + a._23*b._12 + a._33*b._13 + a._43*b._14;
	_14 = a._14*b._11 + a._24*b._12 + a._34*b._13 + a._44*b._14;

	_21 = a._11*b._21 + a._21*b._22 + a._31*b._23 + a._41*b._24;
	_22 = a._12*b._21 + a._22*b._22 + a._32*b._23 + a._42*b._24;
	_23 = a._13*b._21 + a._23*b._22 + a._33*b._23 + a._43*b._24;
	_24 = a._14*b._21 + a._24*b._22 + a._34*b._23 + a._44*b._24;

	_31 = a._11*b._31 + a._21*b._32 + a._31*b._33 + a._41*b._34;
	_32 = a._12*b._31 + a._22*b._32 + a._32*b._33 + a._42*b._34;
	_33 = a._13*b._31 + a._23*b._32 + a._33*b._33 + a._43*b._34;
	_34 = a._14*b._31 + a._24*b._32 + a._34*b._33 + a._44*b._34;

	_41 = a._11*b._41 + a._21*b._42 + a._31*b._43 + a._41*b._44;
	_42 = a._12*b._41 + a._22*b._42 + a._32*b._43 + a._42*b._44;
	_43 = a._13*b._41 + a._23*b._42 + a._33*b._43 + a._43*b._44;
	_44 = a._14*b._41 + a._24*b._42 + a._34*b._43 + a._44*b._44;

	return;
}

void Fmatrix::setHPB(float h, float p, float b)
{
	float sh = std::sin(h);
	float ch = std::cos(h);
	float sp = std::sin(p);
	float cp = std::cos(p);
	float sb = std::sin(b);
	float cb = std::cos(b);

#if 1
	_11 = ch*cb - sh*sp*sb;
	_12 = -cp*sb;
	_13 = ch*sb*sp + sh*cb;
	_14 = 0;

	_21 = sp*sh*cb + ch*sb;
	_22 = cb*cp;
	_23 = sh*sb - sp*ch*cb;
	_24 = 0;

	_31 = -cp*sh;
	_32 = sp;
	_33 = ch*cp;
	_34 = 0;

	_41 = 0;
	_42 = 0;
	_43 = 0;
	_44 = float(1);
#else
	i.set(ch*cb - sh*sp*sb, -cp*sb, ch*sb*sp + sh*cb); _14 = 0;
	j.set(sp*sh*cb + ch*sb, cb*cp, sh*sb - sp*ch*cb); _24 = 0;
	k.set(-cp*sh, sp, ch*cp); _34 = 0;
	c.set(0, 0, 0); _44 = T(1);
#endif

	return;
}

typedef enum
{
	mcAnyMove = 0x01,
	mcCrouch  = 0x02
} ACTOR_DEFS;

bool isActorAccelerated(u32 state, bool b_zoom)
{
	return false;
}

static struct
{
	u64 time_frequency;
	u64 time_old;

	float fTimeDelta; // milliseconds
} Device;

bool fsimilar(float a, float b)
{
	return fabs(a - b) < 0.0001;
}

/* metro specific types */
struct matrix_43T
{
	union {
		struct {
			float _11, _12, _13, _14;
			float _21, _22, _23, _24;
			float _31, _32, _33, _34;
		};
		struct {
			Fvector4 i;
			Fvector4 j;
			Fvector4 k;
		};
	};
	
	// conversion to X-Ray like matrix
	operator Fmatrix() const
	{
		Fmatrix m;
		m._11 = this->_11; m._12 = this->_21; m._13 = this->_31; m._14 = 0.f;
		m._21 = this->_12; m._22 = this->_22; m._23 = this->_32; m._24 = 0.f;
		m._31 = this->_13; m._32 = this->_23; m._33 = this->_33; m._34 = 0.f;
		m._41 = this->_14; m._42 = this->_24; m._43 = this->_34; m._44 = 1.f;
		return m;
	};
	
	// convertsion from X-Ray like matrix
	void operator = (const Fmatrix &m)
	{
		_11 = m._11; _12 = m._21; _13 = m._31; _14 = m._41;
		_21 = m._12; _22 = m._22; _23 = m._32; _24 = m._42;
		_31 = m._13; _32 = m._23; _33 = m._33; _34 = m._43;
	}
};

/* bobbing effector class */
#define CROUCH_FACTOR	0.75f
#define SPEED_REMINDER	5.f 

class CWeaponBobbing
{
	public:
		CWeaponBobbing();
		virtual ~CWeaponBobbing();
		void Load();
		void Update(Fmatrix &m);
		void CheckState();

	private:
		float	fTime;
		Fvector	vAngleAmplitude;
		float	fYAmplitude;
		float	fSpeed;

		u32		dwMState;
		float	fReminderFactor;
		bool	is_limping;
		bool	m_bZoomMode;

		float	m_fAmplitudeRun;
		float	m_fAmplitudeWalk;
		float	m_fAmplitudeLimp;

		float	m_fSpeedRun;
		float	m_fSpeedWalk;
		float	m_fSpeedLimp;
};

CWeaponBobbing::CWeaponBobbing()
{
	Load();
}

CWeaponBobbing::~CWeaponBobbing()
{
}

/*
[wpn_bobbing_effector]
run_amplitude			=	0.0075
walk_amplitude			=	0.005
limp_amplitude			=	0.011
run_speed				=	10.0
walk_speed				=   7.0
limp_speed				=	6.0
*/

void CWeaponBobbing::Load()
{
	fTime			= 0;
	fReminderFactor	= 0;
	is_limping		= false;

	m_fAmplitudeRun		= getFloat(BOBBING_SECT, "run_amplitude", 0.0075f); //pSettings->r_float(BOBBING_SECT, "run_amplitude");
	m_fAmplitudeWalk	= getFloat(BOBBING_SECT, "walk_amplitude", 0.005f); //pSettings->r_float(BOBBING_SECT, "walk_amplitude");
	m_fAmplitudeLimp	= getFloat(BOBBING_SECT, "limp_amplitude", 0.011f); //pSettings->r_float(BOBBING_SECT, "limp_amplitude");

	m_fSpeedRun			= getFloat(BOBBING_SECT, "run_speed", 10.0f); //pSettings->r_float(BOBBING_SECT, "run_speed");
	m_fSpeedWalk		= getFloat(BOBBING_SECT, "walk_speed", 7.0f); //pSettings->r_float(BOBBING_SECT, "walk_speed");
	m_fSpeedLimp		= getFloat(BOBBING_SECT, "limp_speed", 6.0f); //pSettings->r_float(BOBBING_SECT, "limp_speed");
}

void CWeaponBobbing::CheckState()
{
	dwMState		= (GetAsyncKeyState('Y') & 0x8000) ? ACTOR_DEFS::mcAnyMove : 0; // Actor()->get_state();
	is_limping		= false; // Actor()->conditions().IsLimping();
	m_bZoomMode		= false; // Actor()->IsZoomAimingMode();
	fTime			+= Device.fTimeDelta;
}

void CWeaponBobbing::Update(Fmatrix &m)
{
	CheckState();
	if (dwMState&ACTOR_DEFS::mcAnyMove)
	{
		if (fReminderFactor < 1.f)
			fReminderFactor += SPEED_REMINDER * Device.fTimeDelta;
		else						
			fReminderFactor = 1.f;
	}
	else
	{
		if (fReminderFactor > 0.f)
			fReminderFactor -= SPEED_REMINDER * Device.fTimeDelta;
		else			
			fReminderFactor = 0.f;
	}
	if (!fsimilar(fReminderFactor, 0))
	{
		Fvector dangle;
		Fmatrix		R, mR;
		float k		= ((dwMState & ACTOR_DEFS::mcCrouch) ? CROUCH_FACTOR : 1.f);

		float A, ST;

		if (isActorAccelerated(dwMState, m_bZoomMode))
		{
			A	= m_fAmplitudeRun * k;
			ST	= m_fSpeedRun * fTime * k;
		}
		else if (is_limping)
		{
			A	= m_fAmplitudeLimp * k;
			ST	= m_fSpeedLimp * fTime * k;
		}
		else
		{
			A	= m_fAmplitudeWalk * k;
			ST	= m_fSpeedWalk * fTime * k;
		}

		float _sinA	= _abs(_sin(ST) * A) * fReminderFactor;
		float _cosA	= _cos(ST) * A * fReminderFactor;

		m.c.y		+=	_sinA;
		dangle.x	=	_cosA;
		dangle.z	=	_cosA;
		dangle.y	=	_sinA;


		R.setHPB(dangle.x, dangle.y, dangle.z);

		mR.mul		(m, R);

		m.k.set(mR.k);
		m.j.set(mR.j);
	}
}

CWeaponBobbing *g_pWpnBobbing;

/* metro engine hacks */
static void do_bobbing(matrix_43T &hud_matrix)
{
	// update timer

#if 0
	// this won't do well, function is called many times per frame
	u64 time_now;
	u64 time_diff;
	
	QueryPerformanceCounter((PLARGE_INTEGER)&time_now);
	time_diff = time_now - Device.time_old;
	
	Device.fTimeDelta = (float)(((double)time_diff) / ((double)Device.time_frequency));
	Device.time_old = time_now;
#else
	Device.fTimeDelta = 1.f / 240.f;
#endif

	// do bobing
	Fmatrix xr_matrix = hud_matrix;
	g_pWpnBobbing->Update(xr_matrix);
	hud_matrix = xr_matrix;
}

static void __declspec(naked) detour(void)
{
	__asm pusha;
	
	__asm lea eax, [esi+0D0h];
	__asm push eax;
	__asm call do_bobbing;
	__asm add esp, 4;
	
	__asm popa;
	__asm ret;
}

bool install_wpn_bobbing(void)
{
#pragma pack(push, 1)
	struct instruction 
	{
		BYTE  opcode;
		DWORD addr_diff;
	};
#pragma pack(pop)
	
	instruction instr;
	char *func_end_ptr = (char*)0x007115D0;
	
	instr.opcode = 0xE9; // JMP to 32-bit offset
	instr.addr_diff = (char*)detour - (func_end_ptr + sizeof(instr));
	
	ASMWrite(func_end_ptr, (BYTE*)& instr, sizeof(instr));
	
	// initialize timer
	QueryPerformanceFrequency((PLARGE_INTEGER)&Device.time_frequency);
	//Device.time_frequency /= 1000;
	
	// initialize CWeaponBobbing
	g_pWpnBobbing = new CWeaponBobbing;
	
	return true;
}