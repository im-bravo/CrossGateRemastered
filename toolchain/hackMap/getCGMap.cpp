#include <map>
#include <windows.h>
#include <io.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include "getCGMap.h"
#include "utils.h"

CGetCGMap::CGetCGMap()
{
	
}

CGetCGMap::~CGetCGMap()
{

}

void CGetCGMap::mapProc()
{
	// ��ʼ������·��
	init();
	// ��ȡ���е�tile�Լ�object��Ϣ
	readAllTile();

	// ������ͼ·��
	FILE *pFile = nullptr;
	intptr_t hFile = 0;
	struct _finddata_t fileinfo;
	std::string strPath = _strPath + "\\map\\0\\";

	if ((hFile = _findfirst((strPath + "*").c_str(), &fileinfo)) != -1)
	{
		do
		{
			if (fileinfo.attrib & _A_SUBDIR) continue;

			std::string name = fileinfo.name;
			// ������ʱ����
			clearTempData();
			// ��ȡ��ͼ��Ϣ��tileΪ��1��objectΪ��2
			readCGMapData(strPath, name);
			// ����tiledMap��1
			buildTiledData(_mapTileMap, name, TILED_TYPE_TILE);
			// ����tiledMap��2
			buildTiledData(_mapObjectMap, name, TILED_TYPE_OBJECT);
			// ����tile���json����
			buildTileJson();
			// ����obj���json����
			buildObjJson();
			// ���ɶ�Ӧ��ͼ
			buildTiledMap();

		} while (_findnext(hFile, &fileinfo) == 0);
	}
}

void CGetCGMap::init()
{
	_strPath = Utils::getExePath();
	_strTiledMapPath = _strPath + "\\tiledMap\\";
	_strErrFile = _strPath + "\\tiledMap\\err.log";
}

void CGetCGMap::readAllTile()
{
	std::cout << "read fileInfo.json begin..." << std::endl;
	// ��ȡ�ļ���Ϣjson
	std::ifstream ifs((_strPath + "\\newData\\fileInfo.json"));
	if (ifs.good())
	{
		ifs >> _tiledFilesJson;
		ifs.close();
	}
	else
	{
		Utils::saveError(LOG_ERROR, _strErrFile, "read newData\\fileInfo.json failed");
	}
	std::cout << "read fileInfo.json end" << std::endl;
}

void CGetCGMap::clearTempData()
{
	_vTileData.clear();
	_mapTileMap.clear();
	_tileJsonData.clear();
	_vObjectData.clear();
	_mapObjectMap.clear();
	_objJsonData.clear();
}

bool CGetCGMap::readCGMapData(const std::string &strPath, const std::string &strName)
{
	std::cout << "read map data name=" << strName << std::endl;
	// ��ȡ��ͼ��Ϣ
	FILE *pFile = nullptr;
	if (0 == fopen_s(&pFile, (strPath + strName).c_str(), "rb"))
	{
		memset(&_mapHead, 0, sizeof(_mapHead));
		int len = sizeof(tMapHead);
		if (len == fread_s(&_mapHead, len, 1, len, pFile))
		{
			int dataLen = _mapHead.w * _mapHead.h;

			fseek(pFile, 0, SEEK_END);
			int fileLen = ftell(pFile);
			if (fileLen != dataLen * 6 + len)
			{
				return false;
			}

			fseek(pFile, len, SEEK_SET);

			unsigned short n = 0;
			int i = 0;
			while (2 == fread_s(&n, 2, 1, 2, pFile))
			{
				if (i < dataLen)
				{
					// ��ȡ�ر���Ϣ
					_vTileData.push_back((int)n);

					if (n > 0)
						_mapTileMap[n] = 0;
				}
				else if (i < dataLen * 2)
				{
					// �����Ϣ
					_vObjectData.push_back((int)n);

					if (n > 0)
						_mapObjectMap[n] = 0;
				}
				else
				{
					// δ֪��Ϣ
					break;
				}
				++i;
			}
		}
	}
	if (pFile) fclose(pFile);

	return true;
}

void CGetCGMap::buildTiledData(std::map<int, int> &mapData, const std::string strName, int tileType)
{
	std::string strTag;
	switch (tileType)
	{
	case TILED_TYPE_TILE:
	{
		std::cout << "build " << strName << "\'s tile ͼ��json" << std::endl;
		strTag = "tile";
		break;
	}
	case TILED_TYPE_OBJECT:
	{
		std::cout << "build " << strName << "\'s obj ͼ��json" << std::endl;
		strTag = "obj";
		break;
	}
	default:
		break;
	}

	// ��ͼ����·��
	_strMapOnlyName = strName.substr(0, strName.find_last_of('.'));
	std::string strParentPath = _strTiledMapPath + "mapData\\" + _strMapOnlyName + "\\";
	std::string strPath = strParentPath + strTag + "\\";
	Utils::makeSureDirExsits(strPath);

	// ��Ӧ��tiledJson
	// ʹ��tiledmapƴһ�ŵ�ͼ��Ȼ����ݸ�ʽ��д�����ֶ�
	// �����ĺ���û����
	nlohmann::json tiledJson;
	tiledJson["columns"] = 0;
	tiledJson["grid"] = {
		{"orientation", "orthogonal"},
		{"width", 1},
		{"height", 1}
	};
	tiledJson["margin"] = 0;
	tiledJson["spacing"] = 0;
	tiledJson["tiledversion"] = "1.2.2";
	tiledJson["type"] = "tileset";
	tiledJson["version"] = 1.2;
	tiledJson["name"] = _strMapOnlyName + "_" + strTag;
	tiledJson["tiles"] = nlohmann::json::array();
	tiledJson["tilewidth"] = 0;
	tiledJson["tileheight"] = 0;

	// ��ʼ���ļ�
	std::ifstream ifs;
	std::ofstream ofs;
	int idx = 0;
	for (auto &item : mapData)
	{
		if (_tiledFilesJson.find(std::to_string(item.first)) == _tiledFilesJson.end())
		{
			std::string err = "read tiledinfo failed tiled id=" + std::to_string(item.first);
			Utils::saveError(LOG_ERROR, _strErrFile, err);

			continue;
		}

		auto jsonItem = _tiledFilesJson[std::to_string(item.first)];

		// ����ͼƬ����Ӧ·����
		std::string inName = jsonItem["fullName"];
		std::string outName = strPath + Utils::getFileName(inName);

		ifs.open(inName.c_str(), std::ios::binary | std::ios::in | std::ios::_Nocreate);
		ofs.open(outName.c_str(), std::ios::binary | std::ios::out | std::ios::trunc);

		if (ifs.good() && ofs.good())
		{
			ofs << ifs.rdbuf();
		}

		ifs.close();
		ofs.close();

		// �޸ĵر�ӳ��
		item.second = idx + 1;

		// �޸�json
		int imgW = tiledJson["tilewidth"].get<int>();
		int imgH = tiledJson["tileheight"].get<int>();
		int curImgW = jsonItem["width"].get<int>();
		int curImgH = jsonItem["height"].get<int>();

		tiledJson["tilewidth"] = (std::max)(imgW, curImgW);
		tiledJson["tileheight"] = (std::max)(imgH, curImgH);
		// ����ͼ����
		tiledJson["tiles"].push_back({
			{"id", idx},
			{"image", strTag + "/" + Utils::getFileName(inName)},
			{"imagewidth", jsonItem["width"]},
			{"imageheight", jsonItem["height"]}
		});

		++idx;

		// �޸ĵر�ӳ��
		item.second = idx;
	}

	tiledJson["tilecount"] = idx;

	ofs.open((strParentPath + _strMapOnlyName + "_" + strTag + ".json").c_str());
	if (ofs.good())
	{
		ofs << std::setw(4) << tiledJson << std::endl;
		ofs.close();
	}

	// ����tile�Լ�obj�����·������
	switch (tileType)
	{
	case TILED_TYPE_TILE: 
		_strTileJsonName = "mapData\\" + _strMapOnlyName + "\\" + _strMapOnlyName + "_" + strTag + ".json";
		break;
	case TILED_TYPE_OBJECT:
		_strObjJsonName = "mapData\\" + _strMapOnlyName + "\\" + _strMapOnlyName + "_" + strTag + ".json";
		break;
	default:
		break;
	}
}

void CGetCGMap::buildTileJson()
{
	std::cout << "build map tile jsondata" << std::endl;

	// �ر��������
	_tileJsonData["width"] = _mapHead.h;  // ע�������Ƿ���
	_tileJsonData["height"] = _mapHead.w;
	_tileJsonData["name"] = "layer"; // ���д����Ҫд��utf-8��ʽ
	_tileJsonData["id"] = 1;
	_tileJsonData["opacity"] = 1;
	_tileJsonData["type"] = "tilelayer";
	_tileJsonData["visible"] = true;
	_tileJsonData["x"] = 0;
	_tileJsonData["y"] = 0;
	_tileJsonData["data"] = nlohmann::json::array();

	// ��ħ����ͼתΪw*h�Ķ�ά���飬��Ҫ˳ʱ����ת90��������tiled map
	int len = _mapHead.w * _mapHead.h;
	for (int i = 0; i < len; ++i)
	{
		int row = i % _mapHead.h;
		int col = _mapHead.w - i / _mapHead.h - 1;
		int idx = row * _mapHead.w + col;

		_tileJsonData["data"].push_back(_mapTileMap[_vTileData[idx]]);
	}
}

void CGetCGMap::buildObjJson()
{
	std::cout << "build map obj jsondata" << std::endl;

	_objJsonData["drwaorder"] = "topdown";
	_objJsonData["id"] = 1;
	_objJsonData["name"] = "obj";
	_objJsonData["opacity"] = 1;
	_objJsonData["type"] = "objectgroup";
	_objJsonData["visible"] = true;
	_objJsonData["x"] = 0;
	_objJsonData["y"] = 0;
	_objJsonData["objects"] = nlohmann::json::array();

	// tiled map�����������м����ϱߵ���һ�����ӵ��ϱ�Ϊ0��0
	// x������tiled map��������--���£���Ӧ�����ǵ������о���row������
	// Ҳ����row=-1,col=wʱ������Ϊ0,0
	// row=0,col=w-1ʱ�����Ϊ47,47 ����д����������С����Ƭֱ��ƴ�ӾͿ��ԣ���Ƭ����
	int idxBegin = (_mapTileMap.size() / 1000 + 1) * 1000 - 1; // ����ע��-1

	int id = 0;
	for (int i = 0; i < (int)_vObjectData.size(); ++i)
	{
		int objId = _vObjectData[i];
		if (objId <= 0) continue;

		if (_tiledFilesJson.find(std::to_string(objId)) == _tiledFilesJson.end())
		{
			std::string err = "read tiledinfo failed object id=" + std::to_string(objId);
			Utils::saveError(LOG_ERROR, _strErrFile, err);

			continue;
		}
		auto jsonItem = _tiledFilesJson[std::to_string(objId)];

		int row = i / _mapHead.w;
		int col = i % _mapHead.w;


		/**
		 * ԭʼ��ͼ�У����ó���ǰ��������ĵ�Ϊê�㣬ͼƬ���ϽǷŵ�ê�㣬Ȼ��x��y���϶�Ӧ��ƫ�ƿ�ʼ����
		   ��tiled map�У������ֱ�ӷŵ���Ӧλ�ã�������ͼƬ����ڸ��������ĵ�(ê��)��ƫ��Ϊ -w/2, 23-h
		   ��ƫ��Ӧ��ΪxOffset�Լ�yOffset��������Ӧ�Ĵ���

		   ��������Ҫ��ƫ��xTemp = xOffset-(-w/2)
		   ��Ϊ��ͼtile��СΪ64*47��
		   ���xTemp = -64����x += -47�� y -= -47
		   ���xTemp = 64����x += 47, y -= 47
		   ����Ҳ���� x += (xOffset + w / 2) / 64 * 47, y -= x += (xOffset + w / 2) / 64 * 47

		   ��������Ҫ��ƫ��yTemp = yOffset-(23-h)
		   yTempΪ������ (x,y) += |yTemp|��Ϊ������(x,y) += |yTemp| ����Ҳ����(x,y) += (yOffset-(23-h))
		 */
		double xOffset = (jsonItem["xOffset"].get<int>() + jsonItem["width"].get<int>() / 2.0) / 64.0 * 47.0;
		double yOffset = (jsonItem["yOffset"].get<int>() + jsonItem["height"].get<int>() - 13); // 13 magic number...


		_objJsonData["objects"].push_back({
			{"gid", _mapObjectMap[objId] + idxBegin},
			{"id", ++id},
			{"name", ""},
			{"rotation", 0},
			{"type", ""},
			{"visibale", true},
			{"width", jsonItem["width"]},
			{"height", jsonItem["height"]},
			{"x", (row + 1) * 47 + yOffset + xOffset },
			{"y", (_mapHead.w - col) * 47 + yOffset - xOffset }
		});
	}
}

void CGetCGMap::bulldBlockJson()
{
	std::cout << "build map block json" << std::endl;
}

void CGetCGMap::buildTiledMap()
{
	std::cout << "build tiled Map json" << std::endl;

	nlohmann::json tiledMapJson;
	tiledMapJson["width"] = _mapHead.h; // ע�������Ƿ���
	tiledMapJson["height"] = _mapHead.w;
	tiledMapJson["infinite"] = false;
	tiledMapJson["orientation"] = "isometric";
	tiledMapJson["renderorder"] = "left-up";
	tiledMapJson["tileversion"] = "1.2.2";
	tiledMapJson["tilewidth"] = 64;		// ����д��������ĵ�ͼͼ��ܴ��Լ�ƴ�Ϳ�����
	tiledMapJson["tileheight"] = 47;
	tiledMapJson["type"] = "map";
	tiledMapJson["version"] = 1.2;

	tiledMapJson["tilesets"] = nlohmann::json::array();
	tiledMapJson["tilesets"].push_back({
		{"firstgid", 1},
		{"source", _strTileJsonName}
	});
	tiledMapJson["tilesets"].push_back({
		{"firstgid", (_mapTileMap.size() / 1000 + 1) * 1000},
		{"source", _strObjJsonName}
	});

	tiledMapJson["layers"] = nlohmann::json::array();
	tiledMapJson["layers"].push_back(_tileJsonData);
	tiledMapJson["layers"].push_back(_objJsonData);

	std::ofstream ofs;
	ofs.open((_strTiledMapPath + _strMapOnlyName + "_map.json").c_str());
	if (ofs.good())
	{
		ofs << std::setw(4) << tiledMapJson << std::endl;
		ofs.close();
	}
}