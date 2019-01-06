/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "layers.h"

CLayers::CLayers()
{
	m_GroupsNum = 0;
	m_GroupsStart = 0;
	m_LayersNum = 0;
	m_LayersStart = 0;
	m_pGameGroup = 0;
	mem_zero(m_apGameLayers, sizeof(m_apGameLayers));
	m_pVanillaLayer = 0;
	m_pMap = 0;
}

void CLayers::Init(class IKernel *pKernel, IMap *pMap)
{
	m_pMap = pMap ? pMap : pKernel->RequestInterface<IMap>();
	m_pMap->GetType(MAPITEMTYPE_GROUP, &m_GroupsStart, &m_GroupsNum);
	m_pMap->GetType(MAPITEMTYPE_LAYER, &m_LayersStart, &m_LayersNum);

	for(int g = 0; g < NumGroups(); g++)
	{
		CMapItemGroup *pGroup = GetGroup(g);
		for(int l = 0; l < pGroup->m_NumLayers; l++)
		{
			CMapItemLayer *pLayer = GetLayer(pGroup->m_StartLayer+l);

			if(pLayer->m_Type == LAYERTYPE_TILES)
			{
				CMapItemLayerTilemap *pTilemap = reinterpret_cast<CMapItemLayerTilemap *>(pLayer);
				if(pTilemap->m_Flags&TILESLAYERFLAG_GAME)
				{
					m_pVanillaLayer = pTilemap;
					m_pGameGroup = pGroup;

					// make sure the game group has standard settings
					m_pGameGroup->m_OffsetX = 0;
					m_pGameGroup->m_OffsetY = 0;
					m_pGameGroup->m_ParallaxX = 100;
					m_pGameGroup->m_ParallaxY = 100;

					if(m_pGameGroup->m_Version >= 2)
					{
						m_pGameGroup->m_UseClipping = 0;
						m_pGameGroup->m_ClipX = 0;
						m_pGameGroup->m_ClipY = 0;
						m_pGameGroup->m_ClipW = 0;
						m_pGameGroup->m_ClipH = 0;
					}

					break;
				}
			}
		}
	}

	// load ddrace layers
	mem_zero(m_apGameLayers, sizeof(m_apGameLayers));
	int DDRLayersStart, DDRLayersNum;
	m_pMap->GetType(MAPITEMTYPE_DDR_LAYER, &DDRLayersStart, &DDRLayersNum);
	for (int i = 0; i < DDRLayersNum; ++i)
	{
		CMapItemLayerTilemap *pDDRTilemapItem = reinterpret_cast<CMapItemLayerTilemap *>(m_pMap->GetItem(DDRLayersStart + i, 0, 0));
		if(pDDRTilemapItem)
			m_apGameLayers[pDDRTilemapItem->m_Flags] = pDDRTilemapItem;
	}
}

CMapItemLayerTilemap *CLayers::GameLayer(int GameLayerType) const
{
	CMapItemLayerTilemap *pTileMap = m_apGameLayers[GameLayerType];
	if (!pTileMap && GameLayerType == GAMELAYERTYPE_COLLISION)
		pTileMap = m_pVanillaLayer;
	return pTileMap;
}

CMapItemGroup *CLayers::GetGroup(int Index) const
{
	return static_cast<CMapItemGroup *>(m_pMap->GetItem(m_GroupsStart+Index, 0, 0));
}

CMapItemLayer *CLayers::GetLayer(int Index) const
{
	return static_cast<CMapItemLayer *>(m_pMap->GetItem(m_LayersStart+Index, 0, 0));
}
