#include <iostream>
#include <windows.h>
#include <io.h>
#include <sstream>
#include <fstream>
#include "getCGImage.h"
#include "gdiImg.h"

CGetCGImage::CGetCGImage()
{
	// ��ȡ����·��
	_strPath = Utils::getExePath();
}

CGetCGImage::~CGetCGImage()
{
	SAFE_DELETE_A(_imgEncode);
	SAFE_DELETE_A(_imgData);
	SAFE_DELETE_A(_imgPixel);
}

void CGetCGImage::doRun()
{
	readCgp();
	for (auto &item : g_ImgMap)
	{
		readInfo(item.first);
		readAndSaveImg(item.second);
		_vecImginfo.clear();
	}
	saveFileJson();
}

void CGetCGImage::clearData()
{
	_uMapCgp.clear();
	_vecImginfo.clear();
}

void CGetCGImage::readCgp()
{
	// ����palĿ¼�µ������ļ�
	// ��Ŀ¼�µĵ�ɫ��ʵ����ֻ����236����ɫ����Ӧ��ͼƬΪ16-252
	FILE *pFile = nullptr;
	intptr_t hFile = 0;
	struct _finddata_t fileinfo;
	std::string strPath = _strPath + "\\bin\\pal\\";

	if ((hFile = _findfirst((strPath + "*").c_str(), &fileinfo)) != -1)
	{
		do
		{
			if (fileinfo.attrib & _A_SUBDIR) continue;

			std::string name = fileinfo.name;
			if (0 == fopen_s(&pFile, (strPath + name).c_str(), "rb"))
			{
				// ֱ�Ӱѵ�ɫ���д��������
				std::array<unsigned char, DEFAULT_CPG_LEN> c;
				unsigned char *p = c.data();
				if (708 == fread_s(p + 16 * 3, 708, 1, 708, pFile))
				{
					// ��Ĭ�ϵĵ�ɫ��д�� ǰ16��16
					memcpy(p, g_c0_15, 16 * 3);
					memcpy(p + 240 * 3, g_c240_245, 16 * 3);
					_uMapCgp[name] = c;
				}
				else
				{
					// ��ɫ�����
				}
			}

			if (pFile) fclose(pFile);

		} while (_findnext(hFile, &fileinfo) == 0);
	}
}

void CGetCGImage::readInfo(const std::string &strInfo)
{
	FILE *pFile = nullptr;
	std::string strPath = _strPath + "\\bin\\";
	if (0 == fopen_s(&pFile, (strPath + strInfo).c_str(), "rb"))
	{
		imgInfoHead tHead = { 0 };
		int len = sizeof(imgInfoHead);
		while (len == fread_s(&tHead, len, 1, len, pFile))
		{
			_vecImginfo.push_back(tHead);

			if (tHead.tileId != 0)
			{
				_tiledFilesJson[std::to_string(tHead.tileId)] = {
					{"id", tHead.id},
					{"width", tHead.width},
					{"height", tHead.height},
					{"xOffset", tHead.xOffset},
					{"yOffset", tHead.yOffset},
					{"tileEast", tHead.tileEast},
					{"tileSouth", tHead.tileSouth},
					{"flag", tHead.flag}
				};
			}
		}
	}
	if (pFile) fclose(pFile);

	std::cout << "readImgInfo: " << strInfo << " end" << std::endl;
}

void CGetCGImage::readAndSaveImg(const std::string &strName)
{
	// ��ȡͼƬ
	FILE *pFile = nullptr;
	std::string strPath = _strPath + "\\bin\\";

	if (0 != fopen_s(&pFile, (strPath + strName).c_str(), "rb"))
		return;

	// ��¼������־
	std::string strErrorFile = _strPath + "\\newData\\";
	Utils::makeSureDirExsits(strErrorFile);
	strErrorFile += "error.log";

	// ���ɵ��ļ�Ŀ¼
	std::string strSavePath = _strPath + "\\newData\\" + strName.substr(0, strName.find_last_of(".")) + "\\";

	for (auto &ii : _vecImginfo)
	{
		if (getImgData(pFile, ii, strName, strErrorFile))
		{
			std::string strCgp = filleImgPixel(ii.width, ii.height);
			saveImgData(strCgp, strSavePath, ii);
		}
	}

	if (pFile) fclose(pFile);
}

bool CGetCGImage::getImgData(FILE *pFile, const imgInfoHead &imgHead, const std::string &strName, const std::string &strErrorFile)
{
	if (!pFile) return false;

	// ��λ��ͼƬλ��
	fseek(pFile, imgHead.addr, SEEK_SET);

	// ȡ����ӦͼƬ����ͷ
	imgData tHead = { 0 };
	int len = sizeof(imgData);
	
	if (len == fread_s(&tHead, len, 1, len, pFile))
	{
		// �����Ǵ����ͼ
		if (tHead.width > 5000 || tHead.height > 5000)
		{
			saveLog(LOG_ERROR, strErrorFile, strName, "img w or h error", imgHead, tHead);

			return false;
		}


		_cgpLen = 0; // ��ɫ�峤��
		if (tHead.cVer == 3)
		{
			// ���ȡ4���ֽڣ�������ǵ�ɫ��ĳ���
			if (4 != fread_s(&_cgpLen, 4, 1, 4, pFile))
			{
				saveLog(LOG_ERROR, strErrorFile, strName, "read cgpLen error", imgHead, tHead);
				return false;
			}

			len += 4;
		}

		int imgLen = imgHead.len - len;
		if (imgLen == fread_s(_imgEncode, imgLen, 1, imgLen, pFile))
		{
			if (tHead.cVer == 0)
			{
				// δѹ��ͼƬ 
				_imgDataIdx = imgLen;
				memcpy(_imgData, _imgEncode, imgLen);
			}
			else if (tHead.cVer == 1 || tHead.cVer == 3)
			{
				// ѹ����ͼƬ
				_imgDataIdx = decodeImgData(_imgEncode, imgLen);
				if (_imgDataIdx != tHead.width * tHead.height + _cgpLen)
				{
					// ���������˵�Ǵ��
					if (_imgDataIdx < tHead.width * tHead.height + _cgpLen)
					{
						saveLog(LOG_ERROR, strErrorFile, strName, "decode len more", imgHead, tHead);
						return false;
					}
					else
					{
						// ���ڵĻ�Ӧ�����ǲ����Ͻ�
						saveLog(LOG_INFO, strErrorFile, strName, "decode len less", imgHead, tHead);
					}
				}
			}
		}
	}

	return true;
}

std::string CGetCGImage::filleImgPixel(int w, int h)
{
	std::string strCgpName;

	memset(_imgPixel, 0, sizeof(_imgPixel) * sizeof(unsigned int));

	// Ĭ��ʹ��palet_08.cgp(����) ��ɫ��
	unsigned char *pCgp = _uMapCgp.begin()->second.data();
	strCgpName = _uMapCgp.begin()->first;
	// ʹ��ͼƬ�Դ���ɫ��
	if (_cgpLen > 0 && (int)_imgDataIdx >= w * h + _cgpLen)
	{
		pCgp = _imgData + (_imgDataIdx - _cgpLen);
		strCgpName = "self";
	}

	// ͼƬ���ݣ��������Ƿ��ģ������һ�п�ʼ
	int imgLen = w * h;
	for (int i = 0; i < imgLen; ++i)
	{
		// ��ɫ����
		int cIdx = _imgData[i] * 3;
		int idx = (h - i / w - 1) * w + i % w;

		_imgPixel[idx] = (pCgp[cIdx]) + (pCgp[cIdx + 1] << 8) + (pCgp[cIdx + 2] << 16);
		if (pCgp[cIdx] != 0 || pCgp[cIdx + 1] != 0 || pCgp[cIdx + 2] != 0)
			_imgPixel[idx] |= 0xff000000;
	}

	return std::move(strCgpName);
}

void CGetCGImage::saveImgData(const std::string &cgpName, const std::string &strPath, const imgInfoHead &tHead)
{
	// �洢_vecImgData
	// newData/name/cgp/*.png

	FILE *pFile = nullptr;

	// ���ɲ�ͬ��Ŀ¼����ͼ�ļ�����һ��Ŀ¼
	int rangeBegin = tHead.id / 20000;
	std::string strSaveName = strPath;
	if (tHead.tileId == 0)
		strSaveName += std::to_string(rangeBegin * 20000) + "--" + std::to_string(rangeBegin * 20000 + 19999) + "\\";
	else
		strSaveName += "tiled\\" + std::to_string(tHead.tileId) + "_";

	strSaveName += cgpName + "_" + std::to_string(tHead.id);

	Utils::makeSureDirExsits(Utils::extractFileDir(strSaveName));

	_tiledFilesJson[std::to_string(tHead.tileId)]["fullName"] = strSaveName + ".png";

	CGdiSaveImg::getInstance()->saveImage(_imgPixel, tHead.width, tHead.height, strSaveName, "png");

	std::cout << "createImg: id = " << tHead.id << " name = " << strSaveName << std::endl;
}

void CGetCGImage::saveFileJson()
{
	std::ofstream ofs((_strPath + "\\newData\\fileInfo.json").c_str(), std::ios::out | std::ios::trunc);
	if (ofs.bad())
		return;

	ofs << _tiledFilesJson.dump(4);
	ofs.close();
}

int CGetCGImage::decodeImgData(unsigned char *p, int len)
{
	// ͼƬ���� Run-Lengthѹ��
	int iPos = 0;
	int idx = 0;
	while (iPos < len)
	{
		switch (p[iPos] & 0xF0)
		{
		case 0x00:
		{
			// 0x0n �ڶ����ֽ�c����������n���ַ�
			int count = p[iPos] & 0x0F;
			++iPos;
			for (int i = 0; i < count; ++i)
				_imgData[idx++] = p[iPos++];
		}
		break;
		case 0x10:
		{
			// 0x1n �ڶ����ֽ�x���������ֽ�c������n*0x100+x���ַ�
			int count = (p[iPos] & 0x0F) * 0x100 + p[iPos + 1];
			iPos += 2;
			for (int i = 0; i < count; ++i)
				_imgData[idx++] = p[iPos++];
		}
		break;
		case 0x20:
		{
			// 0x2n �ڶ����ֽ�x���������ֽ�y�����ĸ��ֽ�c������n*0x10000+x*0x100+y���ַ�
			int count = (p[iPos] & 0x0F) * 0x10000 + p[iPos + 1] * 0x100 + p[iPos + 2];
			iPos += 3;
			for (int i = 0; i < count; ++i)
				_imgData[idx++] = p[iPos++];
		}
		break;
		case 0x80:
		{
			// 0x8n �ڶ����ֽ�X����������n��X
			int count = p[iPos] & 0x0F;
			for (int i = 0; i < count; ++i)
				_imgData[idx++] = p[iPos + 1];
			iPos += 2;
		}
		break;
		case 0x90:
		{
			// 0x9n �ڶ����ֽ�X���������ֽ�m����������n*0x100+m��X
			int count = (p[iPos] & 0x0F) * 0x100 + p[iPos + 2];
			for (int i = 0; i < count; ++i)
				_imgData[idx++] = p[iPos + 1];
			iPos += 3;
		}
		break;
		case 0xa0:
		{
			// 0xan �ڶ����ֽ�X���������ֽ�m�����ĸ��ֽ�z����������n*0x10000+m*0x100+z��X
			int count = (p[iPos] & 0x0F) * 0x10000 + p[iPos + 2] * 0x100 + p[iPos + 3];
			for (int i = 0; i < count; ++i)
				_imgData[idx++] = p[iPos + 1];
			iPos += 4;
		}
		break;
		case 0xc0:
		{
			// 0xcn ͬ0x8n��ֻ���������Ǳ���ɫ
			int count = p[iPos] & 0x0F;
			for (int i = 0; i < count; ++i)
				_imgData[idx++] = 0;
			iPos += 1;
		}
		break;
		case 0xd0:
		{
			// 0xdn ͬ0x9n��ֻ���������Ǳ���ɫ
			int count = (p[iPos] & 0x0F) * 0x100 + p[iPos + 1];
			for (int i = 0; i < count; ++i)
				_imgData[idx++] = 0;
			iPos += 2;
		}
		break;
		case 0xe0:
		{
			int count = (p[iPos] & 0x0F) * 0x10000 + p[iPos + 1] * 0x100 + p[iPos + 2];
			for (int i = 0; i < count; ++i)
				_imgData[idx++] = 0;
			iPos += 3;
		}
		break;
		default:
			break;
		}
	}

	return idx;
}

void CGetCGImage::saveLog(int logLevel, const std::string &strErrorFile, const std::string &strName, const std::string &strTag,
	const imgInfoHead &tIdxHead, const imgData &tImgData)
{
	std::ostringstream ostr;
	ostr << strTag << " file=" << strName
		<< " id=" << tIdxHead.id << " tileId=" << tIdxHead.tileId
		<< " ver=" << (int)tImgData.cVer << " w=" << tImgData.width << " h=" << tImgData.height
		<< " imgHeadLen=" << tImgData.len << " cgpLen=" << _cgpLen
		<< " decodeLen=" << _imgDataIdx << " expectLen=" << tImgData.width * tImgData.height;

	Utils::saveError((eLogLevel)logLevel, strErrorFile, ostr.str());
}