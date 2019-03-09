#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <array>
#include "define.h"

class CGetCGImage
{
public:
	CGetCGImage();
	~CGetCGImage();

public:
	void doRun();

private:
	void clearData();
	void readCgp();
	void readInfo(const std::string &strInfo);
	void readAndSaveImg(const std::string &strName);

private:
	bool getImgData(FILE *pFile, const imgInfoHead &imgHead, const std::string &strName, const std::string &strErrorFile);
	std::string filleImgPixel(int w, int h);
	void saveImgData(const std::string &cgpName, const std::string &strPath, const imgInfoHead &tHead);

	int decodeImgData(unsigned char *p, int len);
	bool isNewFormat(const std::string &strName)
	{
		if (strName == "Graphic_20.bin" || strName == "GraphicEx_4.bin")
			return false;

		return true;
	}

private:
	std::string _strPath;	// ����·��
	std::unordered_map<std::string, std::array<unsigned char, DEFAULT_CPG_LEN>> _uMapCgp; // ��ɫ��
	std::vector<imgInfoHead> _vecImginfo; // ͼƬ����

	unsigned char *_imgEncode = new unsigned char[1024 * 1024 + 256 * 3]; // ��¼���ܺ��ͼƬ��Ϣ

	unsigned char *_imgData = new unsigned char[1024 * 1024 + 256 * 3]; // ��¼���ܺ��ͼƬ���ݣ��еĴ��е�ɫ�壬��ɫ���¼�����
	unsigned int _imgDataIdx = 0;	// ����֮���idx

	unsigned int *_imgPixel = new unsigned int[1024 * 1024]; // ��¼ͼƬ���� ���֧��4M��ͼƬ �����ͼƬ�����޸�����
};