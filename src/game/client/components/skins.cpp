/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <math.h>

#include <base/system.h>
#include <base/math.h>

#include <engine/graphics.h>
#include <engine/storage.h>
#include <engine/shared/config.h>

#include "skins.h"

const char* vanillaSkins[] = {"bluekitty.png", "bluestripe.png", "brownbear.png",
	"cammo.png", "cammostripes.png", "coala.png", "default.png", "limekitty.png",
	"pinky.png", "redbopp.png", "redstripe.png", "saddo.png", "toptri.png",
	"twinbop.png", "twintri.png", "warpaint.png", "x_ninja.png"};



int CSkins::CSkin::GetColorTexture() const
{
	if(m_ColorTexture == SKIN_TEXTURE_NOT_LOADED)
		m_pSkins->LoadTextures(const_cast<CSkin*>(this)); // const-cheatsy allowed here because this should look like a plain getter to the outside

	return m_ColorTexture;
}

int CSkins::CSkin::GetOrgTexture() const
{
	if(m_OrgTexture == SKIN_TEXTURE_NOT_LOADED)
		m_pSkins->LoadTextures(const_cast<CSkin*>(this));

	return m_OrgTexture;
}


void CSkins::LoadTextures(CSkin *pSkin)
{
	pSkin->m_ColorTexture = CSkin::SKIN_TEXTURE_NOT_FOUND;
	pSkin->m_OrgTexture = CSkin::SKIN_TEXTURE_NOT_FOUND;

	CImageInfo Info;
	if(!Graphics()->LoadPNG(&Info, pSkin->m_FileInfo.m_aFullPath, pSkin->m_FileInfo.m_DirType))
	{
		Console()->Printf(IConsole::OUTPUT_LEVEL_ADDINFO, "game", "failed to load skin from %s", pSkin->m_FileInfo.m_aFullPath);
		return;
	}

	pSkin->m_OrgTexture = Graphics()->LoadTextureRaw(Info.m_Width, Info.m_Height, Info.m_Format, Info.m_pData, Info.m_Format, 0);

	int BodySize = 96; // body size
	if (BodySize > Info.m_Height)
		return;

	unsigned char *d = (unsigned char *)Info.m_pData;
	int Pitch = Info.m_Width*4;

	// dig out blood color
	{
		int aColors[3] = {0};
		for(int y = 0; y < BodySize; y++)
			for(int x = 0; x < BodySize; x++)
			{
				if(d[y*Pitch+x*4+3] > 128)
				{
					aColors[0] += d[y*Pitch+x*4+0];
					aColors[1] += d[y*Pitch+x*4+1];
					aColors[2] += d[y*Pitch+x*4+2];
				}
			}

		pSkin->m_BloodColor = normalize(vec3(aColors[0], aColors[1], aColors[2]));
	}

	// create colorless version
	int Step = Info.m_Format == CImageInfo::FORMAT_RGBA ? 4 : 3;

	// make the texture gray scale
	for(int i = 0; i < Info.m_Width*Info.m_Height; i++)
	{
		int v = (d[i*Step]+d[i*Step+1]+d[i*Step+2])/3;
		d[i*Step] = v;
		d[i*Step+1] = v;
		d[i*Step+2] = v;
	}


	int Freq[256] = {0};
	int OrgWeight = 0;
	int NewWeight = 192;

	// find most common frequence
	for(int y = 0; y < BodySize; y++)
		for(int x = 0; x < BodySize; x++)
		{
			if(d[y*Pitch+x*4+3] > 128)
				Freq[d[y*Pitch+x*4]]++;
		}

	for(int i = 1; i < 256; i++)
	{
		if(Freq[OrgWeight] < Freq[i])
			OrgWeight = i;
	}

	// reorder
	int InvOrgWeight = 255-OrgWeight;
	int InvNewWeight = 255-NewWeight;
	for(int y = 0; y < BodySize; y++)
		for(int x = 0; x < BodySize; x++)
		{
			int v = d[y*Pitch+x*4];
			if(v <= OrgWeight)
				v = (int)(((v/(float)OrgWeight) * NewWeight));
			else
				v = (int)(((v-OrgWeight)/(float)InvOrgWeight)*InvNewWeight + NewWeight);
			d[y*Pitch+x*4] = v;
			d[y*Pitch+x*4+1] = v;
			d[y*Pitch+x*4+2] = v;
		}

	pSkin->m_ColorTexture = Graphics()->LoadTextureRaw(Info.m_Width, Info.m_Height, Info.m_Format, Info.m_pData, Info.m_Format, 0);
	mem_free(Info.m_pData);

	if(g_Config.m_Debug)
		Console()->Printf(IConsole::OUTPUT_LEVEL_ADDINFO, "game", "loaded skin texture for '%s'", pSkin->m_aName);
}


int CSkins::SkinScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	CALLSTACK_ADD();

	int l = str_length(pName);
	if(l < 4 || IsDir || str_comp(pName+l-4, ".png") != 0)
		return 0;

	CSkin Skin;
	Skin.m_IsVanilla = false;
	for(unsigned int i = 0; i < sizeof(vanillaSkins) / sizeof(vanillaSkins[0]); i++)
	{
		if(str_comp(pName, vanillaSkins[i]) == 0)
		{
			Skin.m_IsVanilla = true;
			break;
		}
	}

	if(g_Config.m_ClVanillaSkinsOnly && !Skin.m_IsVanilla)
		return 0;

	IStorageTW::CLoadHelper<CSkins> *pLoadHelper = (IStorageTW::CLoadHelper<CSkins> *)pUser;
	CSkins *pSelf = pLoadHelper->pSelf;

	Skin.m_pSkins = pSelf;

	// Don't add duplicate skins (one from user's config directory, other from
	// client itself)
	for(int i = 0; i < pSelf->Num(); i++)
	{
		const char *pExName = pSelf->Get(i)->m_aName;
		if(str_comp_num(pExName, pName, l-4) == 0 && str_length(pExName) == l-4)
			return 0;
	}

	Skin.m_FileInfo.m_DirType = DirType;
	str_formatb(Skin.m_FileInfo.m_aFullPath, "%s/%s", pLoadHelper->pFullDir, pName);

	// textures are being loaded on-demand; later when skin is needed
	Skin.m_OrgTexture = CSkin::SKIN_TEXTURE_NOT_LOADED;
	Skin.m_ColorTexture = CSkin::SKIN_TEXTURE_NOT_LOADED;

	// set skin data
	str_copy(Skin.m_aName, pName, min((int)sizeof(Skin.m_aName),l-3));
	int Index = pSelf->m_aSkins.add(Skin);
	if(str_comp_nocase(Skin.m_aName, "default") == 0)
		pSelf->m_DefaultSkinIndex = Index;

	if(g_Config.m_Debug)
		pSelf->Console()->Printf(IConsole::OUTPUT_LEVEL_ADDINFO, "game", "added skin '%s'", Skin.m_aName);

	return 0;
}


void CSkins::OnInit()
{
	CALLSTACK_ADD();

	// load skins
	RefreshSkinList();
}

void CSkins::RefreshSkinList(bool clear)
{
	CALLSTACK_ADD();

	if(clear)
		Clear();

	IStorageTW::CLoadHelper<CSkins> *pLoadHelper = new IStorageTW::CLoadHelper<CSkins>;
	pLoadHelper->pSelf = this;

	pLoadHelper->pFullDir = "skins";
	Storage()->ListDirectory(IStorageTW::TYPE_ALL, "skins", SkinScan, pLoadHelper);

	if(!g_Config.m_ClVanillaSkinsOnly)
	{
		pLoadHelper->pFullDir = "downloadedskins";
		Storage()->ListDirectory(IStorageTW::TYPE_SAVE, "downloadedskins", SkinScan, pLoadHelper);
	}

	if(m_aSkins.empty())
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "gameclient", "failed to load skins. folder='skins/'");
		CSkin DummySkin;
		DummySkin.m_OrgTexture = -1;
		DummySkin.m_ColorTexture = -1;
		str_copy(DummySkin.m_aName, "dummy", sizeof(DummySkin.m_aName));
		DummySkin.m_BloodColor = vec3(1.0f, 1.0f, 1.0f);
		m_DefaultSkinIndex = m_aSkins.add(DummySkin);
	}

	delete pLoadHelper;
}

int CSkins::Num()
{
	CALLSTACK_ADD();

	return m_aSkins.size();
}

const CSkins::CSkin *CSkins::Get(int Index)
{
	return &m_aSkins[max(0, Index%m_aSkins.size())];
}

int CSkins::Find(const char *pName)
{
	CALLSTACK_ADD();

	for(int i = 0; i < m_aSkins.size(); i++)
	{
		if(str_comp(m_aSkins[i].m_aName, pName) == 0)
			return i;
	}
	return -1;
}

void CSkins::Clear()
{
	CALLSTACK_ADD();

	while(!m_aSkins.empty())
	{
		Graphics()->UnloadTexture(m_aSkins[0].m_OrgTexture);
		Graphics()->UnloadTexture(m_aSkins[0].m_ColorTexture);
		m_aSkins.remove_index_fast(0);
	}
	m_aSkins.clear();
	m_DefaultSkinIndex = -1;
}

vec3 CSkins::GetColorV3(int v)
{
	return HslToRgb(vec3(((v>>16)&0xff)/255.0f, ((v>>8)&0xff)/255.0f, 0.5f+(v&0xff)/255.0f*0.5f));
}

vec4 CSkins::GetColorV4(int v)
{
	vec3 r = GetColorV3(v);
	return vec4(r.r, r.g, r.b, 1.0f);
}
