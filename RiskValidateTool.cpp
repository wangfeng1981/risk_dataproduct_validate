#pragma execution_character_set("utf-8")
#include "RiskValidateTool.h"

#define WGDALRASTER_H_IMPLEMENTATION
#include "wGdalRaster.h"
#include "wstringutils.h"

/// V1.0.0
///   第一个版本 2022-1-7
///   （1）判断CGCS2000坐标的规则为，现将输入数据wkt转成大写，然后前面与"GEOGCS["比较，后面与",AUTHORITY[\"EPSG\",\"4490\"]]"比较，
///     全部相同则认为是CGCS2000坐标。
///   （2）分辨率判断使用10位小数进行判断
/// 
/// 

/// V1.0.1
/// （1） checkEveryGridMatching ，范围判断中返回不匹配点数据最多10个

/// V1.0.2
/// (1)使用精度检验考虑9位小数

/// V1.1.0
/// (1)增加栅格数据类型判断，要求是byte，uint16，int16，uint32，int32 这几种类型，危险性等级取值范围0-4，风险等级取值只能0-5
/// (2)缺少的标准网格的rows和cols录入在错误信息中

/// V1.1.1 2022-1-9
/// (1)修复isGeoTiff_GridSizeOK中trans在Raster释放后仍然参与计算的bug，使用transVal进行计算.

/// V1.2.0 2022-1-12
/// (1)修改产品错误说明，精简这些说明内容

/// V1.2.1 2022-1-20
/// (1) NoData 可以不是0
/// (2) 不对CGCS2000进行检验

/// V1.2.2 2022-1-20
/// (1) 图像范围的判断考虑数据本身填充值，不再使用0
///

/// V1.3.0 2022-1-20
/// （1）每个错误增加详细信息输出


/// V1.3.1 2022-1-20
/// （1）坐标检查精度统一使用六位小数 0.000001

/// V1.4.0 2022-1-20
/// (1)增加对shp文件矢量四角范围检查的支持
/// (2)对shp值的有效性检查，检查class字段，检查精度采用六位小数，

/// V1.4.1 2022-1-20
/// (1)修复计算栅格数据范围的bug

/// V1.4.2 2022-1-25
/// (1)修复读取无效shp程序崩溃的bug

string RiskValidateTool::version = "v1.4.2";
double RiskValidateTool::eps =  0.000001;//2022-1-20
string RiskValidateTool::errorDetail = "" ;

bool RiskValidateTool::isGeoTiff_CGCS2000(string rasterfile, string& error)
{
	if( false ){
		//根据用户要求不再对CGCS2000进行判断 2022-1-20
		wGdalRaster* raster = wGdalRasterFactory::OpenFile(rasterfile);
		if (raster == 0) {
			error = "打开文件失败,请检查文件是否是有效的GeoTiff";
			return false;
		}

		string inputWkt = string(raster->getProj()) ;
		inputWkt = wStringUtils::toUpper(inputWkt);

		delete raster;

		
		const string cgcs2000wktHead = "GEOGCS[";
		const string cgcs2000wktTail = ",AUTHORITY[\"EPSG\",\"4490\"]]";

		if (inputWkt.length() < cgcs2000wktHead.length() + cgcs2000wktTail.length()) {
			//error = string("输入文件不是CGCS2000坐标系(1),输入数据wkt为:") + inputWkt;
			error = string("输入文件不是CGCS2000坐标系");
			return false;
		}

		string inputWktHead = inputWkt.substr(0, cgcs2000wktHead.length());
		string inputWktTail = inputWkt.substr(inputWkt.length() - cgcs2000wktTail.length());

		if (cgcs2000wktHead.compare(inputWktHead) != 0 || cgcs2000wktTail.compare(inputWktTail) != 0 )
		{
            errorDetail = string("输入文件不是标准CGCS2000坐标系(2),输入数据wkt为:") + inputWkt;
			error = string("输入文件不是CGCS2000坐标系");
			return false;
		}
	}
	return true;
}


bool RiskValidateTool::isShape_CGCS2000(string shpfile, string& error)
{
	return true;
}

bool RiskValidateTool::isGeoTiff_GridSizeOK(string rasterfile, string& error)
{
	wGdalRaster* raster = wGdalRasterFactory::OpenFile(rasterfile);
	if (raster == 0) {
		error = "打开文件失败,请检查文件是否是有效的GeoTiff";
		return false;
	}

	const double* trans = raster->getTrans();
	double transVal[6];
	for (int i = 0; i < 6; ++i) transVal[i] = trans[i];
	delete raster;

	// 30''
	const double thirtySeconds = 30.0 / 3600.0;
	
	bool goodReso = false;
	if (fabs(transVal[2]) < eps && fabs(transVal[4]) < eps)
	{
        if (fabs(thirtySeconds - transVal[1]) < eps && fabs(thirtySeconds + transVal[5]) < eps) //bugfixed 2022-1-9
		{
			goodReso = true;
		}
	}

	if (goodReso == false) {
		char buffer1[64];
		char buffer2[64];
		sprintf(buffer1, "(%15.10f)", thirtySeconds);
		sprintf(buffer2, "(%15.10f,%15.10f)", transVal[1],transVal[5]);
        errorDetail = string("输入栅格数据分辨率不符合30秒") + buffer1 + ",输入数据分辨率:" + buffer2;
		error = string("输入文件格网分辨率不是30''");
		return false;
	}


	return true;
}


bool RiskValidateTool::isShape_GridSizeOK(string shpfile, string& error)
{
	return true;
}

bool RiskValidateTool::isGeoTiff_ExtentOk(string standardfilename,string rasterfilename, string& error)
{
	string fileName = wStringUtils::getFileNameFromFilePath(rasterfilename);
	vector<string> strArr = wStringUtils::splitString(fileName, "_");
	if (strArr.size() < 5) {
		error = string("输入文件文件名不规范:") + fileName + ", 文件名规范为:灾害类别代码_成果类型_数据对象细化代码_6位行政区划编码_序号(01-99).xxx";
		return false;
	}

	string xzcodeStr = strArr[3];
	bool isAllNum = wStringUtils::isAllNumber(xzcodeStr);
	if (isAllNum == false) {
		error = string("行政区划编码无效:") + xzcodeStr;
		return false;
	}
	int xycodei = atof(xzcodeStr.c_str());

	double standleft, standright, standtop, standbottom;
	double inleft, inright, intop, inbottom;
	int standcnt = 0;
	int incnt = 0;

	bool standExtentOk = computeExtentByCode(standardfilename, xycodei, standleft, standright, standtop, standbottom, standcnt, error);
	if (standExtentOk == false) {
		return false;
	}

	bool inExtentOk = computeValidExtent(rasterfilename,   inleft, inright, intop, inbottom, incnt, error);
	if (inExtentOk == false) {
		return false;
	}

	if (standcnt != incnt) {
		//error = string("输入数据网格数量(")+ wStringUtils::int2str(incnt) + ")与行政区标准网格数量(" 
		//	+ wStringUtils::int2str(standcnt) +")不匹配" ;
		error = string("输入文件与标准格网不完全重合");
		return false;
	}

	if (fabs(standleft - inleft) > eps || fabs(standright - inright) > eps ||
		fabs(standtop - intop) > eps || fabs(standbottom - inbottom) > eps)
	{
		char buffer1[128];
		char buffer2[128];
		sprintf(buffer1, "%15.10f,%15.10f,%15.10f,%15.10f", inleft, inright, intop, inbottom);
		sprintf(buffer2, "%15.10f,%15.10f,%15.10f,%15.10f", standleft, standright, standtop, standbottom);
        errorDetail = string("输入数据空间范围(") + string(buffer1) + ")与行政区标准网格空间范围("
            + string(buffer2) + ")不匹配";
		error = string("输入文件与标准格网不完全重合");
		return false;
	}

	vector<RiskValidateToolPointI> unMatchPoints;
	bool matchOk = checkEveryGridMatching(standardfilename, xycodei, rasterfilename,  unMatchPoints, error);
	if (matchOk == false) {
		error = string("输入文件与标准格网不完全重合");
		return false;
	}

	return true;
}


bool RiskValidateTool::isShp_ExtentOk(string standardfilename,string shpfile, string& error)
{
	//检查文件名，获取6位行政区划编码
	string fileName = wStringUtils::getFileNameFromFilePath(shpfile);
	vector<string> strArr = wStringUtils::splitString(fileName, "_");
	if (strArr.size() < 5) {
		error = string("输入文件文件名不规范:") + fileName + ", 文件名规范为:灾害类别代码_成果类型_数据对象细化代码_6位行政区划编码_序号(01-99).xxx";
		return false;
	}

	string xzcodeStr = strArr[3];
	bool isAllNum = wStringUtils::isAllNumber(xzcodeStr);
	if (isAllNum == false) {
		error = string("行政区划编码无效:") + xzcodeStr;
		return false;
	}
	int xycodei = atof(xzcodeStr.c_str());

	//获取标准格网的四角范围
	double standleft(0), standright(0), standtop(0), standbottom(0);
	int standcnt = 0;
	string error1 ;
	bool standExtentOk = computeExtentByCode(standardfilename, xycodei, standleft, 
		standright, standtop, standbottom, standcnt, error1);
	if (standExtentOk == false) {
		error = string("计算标准格网范围失败：")+error1 ;
		return false;
	}

	GDALDataset* poDS = (GDALDataset*) GDALOpenEx( shpfile.c_str() , GDAL_OF_VECTOR, NULL, NULL, NULL );
	if( poDS==nullptr ){
		error = "读取矢量文件失败" ;
		return false ;
	}
	int numLayers = poDS->GetLayerCount() ;
	if( numLayers==0 ){
		error = "读取矢量图层数量失败" ;
		GDALClose(poDS) ;
		return false ;
	}

	OGRLayer* pLayer = poDS->GetLayer(0) ;
	if( pLayer==nullptr ){
		error = "读取矢量图层失败" ;
		GDALClose(poDS) ;
		return false ;
	}

	OGREnvelope env ;
	OGRErr ogrError = pLayer->GetExtent( &env ) ;
	if( ogrError== OGRERR_FAILURE) {
		error = "计算矢量图层范围失败" ;
		GDALClose(poDS) ;
		return false ;
	}

	GDALClose(poDS) ;

	// bugfixed 2022-1-25 env有可能不是经纬度坐标，这会导致sprintf 128的数组崩溃。
	if( fabs(env.MinX)> 1000.0 || fabs(env.MaxX)> 1000.0 || fabs(env.MaxY)> 1000.0 || fabs(env.MinY)> 1000.0  ){
		errorDetail = string("输入数据空间范围超过有效的经纬度范围");
		error = string("输入数据空间范围超过有效的经纬度范围");
		return false ;
	}

	if (fabs(standleft - env.MinX ) > eps || fabs(standright - env.MaxX) > eps ||
		fabs(standtop - env.MaxY) > eps || fabs(standbottom - env.MinY) > eps)
	{
		char buffer1[128];
		char buffer2[128];
		sprintf(buffer1, "%15.10f,%15.10f,%15.10f,%15.10f", env.MinX, env.MaxX, env.MaxY, env.MinY);
		sprintf(buffer2, "%15.10f,%15.10f,%15.10f,%15.10f", standleft, standright, standtop, standbottom);
        errorDetail = string("输入数据空间范围(") + string(buffer1) + ")与行政区标准网格空间范围("
            + string(buffer2) + ")不匹配";
		error = string("输入文件与标准格网不完全重合");
		return false;
	}

	return true;
}


bool RiskValidateTool::isShp_classValueOk(string shpfilename,string& error) //2022-1-20
{
	//检查文件名，获取6位行政区划编码  BY_P_10_130131_01.shp
	string fileName = wStringUtils::getFileNameFromFilePath(shpfilename);
	vector<string> strArr = wStringUtils::splitString(fileName, "_");
	if (strArr.size() < 5) {
		error = string("输入文件文件名不规范:") + fileName + ", 文件名规范为:灾害类别代码_成果类型_数据对象细化代码_6位行政区划编码_序号(01-99).xxx";
		return false;
	}

	bool pcodeOk = wStringUtils::isAllNumber( strArr[2] ) ;
	if( pcodeOk ==false ){
		error = string("输入文件文件名中数据对象细化代码无效:") +strArr[2];
		return false;
	}

	string zhCode = strArr[0] ;

	// 代码01 表示危险性区划 1-4 ， 其他应该都是风险相关 1-5 ，根据9号文，shp文件没有危险性的数据
	// 根据后来普查办的通知，新增三种shp产品，XX危险性评估，XXGDP风险评估，XX人口风险评估 编码按9号文顺序增加
	// 实际上每个灾害只有XX危险性评估是1-4级，其他均是1-5级
	const int ZAIHAI_NUM = 11 ;//灾害总数
	string zhCodeArr[ZAIHAI_NUM] = {"TF","BY","BB", "DF" , "DW" , "GH" , "GW" , "LD" , "XZ" , "SC" , "DISA" } ;
	//危险性产品编码
	int wxxCodeArr[ZAIHAI_NUM] =   {12  ,10  ,10  , 10   , 10   , 10   , 10   , 4    , 10   , 6    , 4      } ;
	int productCode = atof( strArr[2].c_str() ) ;
	// 每个灾害的危险性shp产品编码都不一样
	int validMin = 1 ;//2022-1-20 无效值单独考虑，不放在这里
	int validMax = 5 ;
	string validDesc = "" ;
	bool findZH = false ;
	for(int i = 0 ; i < ZAIHAI_NUM; ++ i  ){
		if( zhCode.compare(zhCodeArr[i]) == 0 ){
			findZH = true ;
			if( productCode == wxxCodeArr[i] ){
				validMax = 4 ;//危险性
				validDesc = "1-4" ;
			}else {
				validMax = 5 ;//风险
				validDesc = "1-5" ;
			}
			break ;
		}
	}

	if( findZH==false ){
		error = string("灾害代码无效:") +zhCode;
		return false ;
	}


	GDALDataset* poDS = (GDALDataset*) GDALOpenEx( shpfilename.c_str() , GDAL_OF_VECTOR, NULL, NULL, NULL );
	if( poDS==nullptr ){
		error = "读取矢量文件失败" ;
		return false ;
	}
	int numLayers = poDS->GetLayerCount() ;
	if( numLayers==0 ){
		error = "读取矢量图层数量失败" ;
		GDALClose(poDS) ;
		return false ;
	}

	OGRLayer* pLayer = poDS->GetLayer(0) ;
	if( pLayer==nullptr ){
		error = "读取矢量图层失败" ;
		GDALClose(poDS) ;
		return false ;
	}

	OGRFeature *poFeature=nullptr ;
	pLayer->ResetReading();
	//逐个要素核对分级数值
	int whileState = 0 ;//0 is all good , 1 is feature no class field , 2  bad value
	while( (poFeature = pLayer->GetNextFeature()) != NULL ){
		//注意GetNextFeature 返回的指针需要调用者释放
		int fieldIndex = poFeature->GetFieldIndex("class") ;
		if ( fieldIndex == -1){
			whileState = 1 ;
		}else{
			//has class
			int clsVal = poFeature->GetFieldAsInteger(fieldIndex) ;
			if( clsVal < validMin || clsVal > validMax ){
				//bad value
				whileState = 2 ;
			}
		}
		OGRFeature::DestroyFeature(poFeature) ;
		if( whileState!=0 ) break ;//跳出循环
	}
	GDALClose(poDS) ;

	if( whileState == 1 ){
		error = "存在矢量要素没有class字段" ;
		return false ;
	}else if( whileState==2 ){
		error = string("存在矢量要素class字段值超出范围:")+validDesc ;
		return false ;
	}
	return true ;
}


//获取行政编码在标准网格中的范围，行政编码不足9位的，在后面补齐，比如110001，进行查找的时候使用110001000~110001999 2022-1-7
//行政区划编码不能是0开头的
bool RiskValidateTool::computeExtentByCode(string standardGridFilename, int code,
	double& retleft, double& retright, double& rettop, double& retbottom, int& retGridCout, string& error)
{
	std::shared_ptr<wGdalRaster> standardGridPtr(wGdalRasterFactory::OpenFile(standardGridFilename));
	if (standardGridPtr.get() == 0) {
		error = "打开标准网格数据文件失败";
		return false;
	}

	int codeLow = 0;
	int codeHigh = 9;
	bool codeOk = computeXzCode(code, codeLow, codeHigh);
	if (codeOk ==false ) {
		error = "行政编码无效:" + wStringUtils::int2str(code);
		return false;
	}

	//for debug
	//cout << "debug code,low,high:" << code << "," << codeLow << "," << codeHigh << endl;

	retGridCout = 0;
	retleft = 999999.0;
	retright = -999999.0;
	rettop = -999999.0;
	retbottom = 999999.0;

	int xsize = standardGridPtr->getXSize();
	int ysize = standardGridPtr->getYSize();
	const double* trans = standardGridPtr->getTrans();
	for (int iy = 0; iy < ysize; ++iy)
	{
		for (int ix = 0; ix < xsize; ++ix)
		{
			int pxval = standardGridPtr->getValuei(ix, iy, 0);
			if (pxval >= codeLow && pxval <= codeHigh)
			{
				++retGridCout;
				double pxleft = trans[0] + trans[1] * ix;
				double pxtop = trans[3] + trans[5] * iy;
				if (retleft > pxleft) retleft = pxleft;
				if (rettop < pxtop) rettop = pxtop;

				//2022-1-20
				double pxright = pxleft + trans[1] ;
				double pxbottom = pxtop + trans[5] ;
				if( retright < pxright ) retright = pxright ;     //bugfixed 448
				if( retbottom > pxbottom ) retbottom = pxbottom ; //bugfixed 449

			}
		}
	}

	if (retGridCout == 0) {
		error = "标准网格中没有找到行政编码:" + wStringUtils::int2str(codeLow) + "~" + wStringUtils::int2str(codeHigh) ;
		return false;
	}else //if(retGridCout > 0) 
	{
		// retright = retleft + trans[1];//bug448, 
		// retbottom = rettop + trans[5];//bug449,
	}
	return true;
}

//获取栅格分级数据有效值范围，fillValue为填充值，计算所有不等于fillValue值的有效像素范围
bool RiskValidateTool::computeValidExtent(string rasterfilename,  
	double& retleft, double& retright, double& rettop, double& retbottom, int& retValidCout, string& error)
{
	std::shared_ptr<wGdalRaster> rasterPtr(wGdalRasterFactory::OpenFile(rasterfilename));
	if (rasterPtr.get() == 0) {
		error = "打开输入栅格数据文件失败";
		return false;
	}

	retValidCout = 0;
	retleft = 999999.0;
	retright = -999999.0;
	rettop = -999999.0;
	retbottom = 999999.0;
	int fillValue = rasterPtr->getNoDataValue() ;//2022-1-20

	int xsize = rasterPtr->getXSize();
	int ysize = rasterPtr->getYSize();
	const double* trans = rasterPtr->getTrans();
	for (int iy = 0; iy < ysize; ++iy)
	{
		for (int ix = 0; ix < xsize; ++ix)
		{
			int pxval = rasterPtr->getValuei(ix, iy, 0);
			if (pxval != fillValue)
			{
				++retValidCout;
				double pxleft = trans[0] + trans[1] * ix;
				double pxtop = trans[3] + trans[5] * iy;
				if (retleft > pxleft) retleft = pxleft;
				if (rettop < pxtop) rettop = pxtop;

				//2022-1-20
				double pxright = pxleft + trans[1] ;
				double pxbottom = pxtop + trans[5] ;
				if( retright < pxright ) retright = pxright ;     //bugfixed 502
				if( retbottom > pxbottom ) retbottom = pxbottom ; //bugfixed 503
			}
		}
	}

	if (retValidCout == 0) {
		error = "输入栅格数据中没有找到有效值" ;
		return false;
	}
	else //if(retGridCout > 0) 
	{
		//retright = retleft + trans[1]; //bug502
		//retbottom = rettop + trans[5]; //bug503
	}
	return true;

}

//逐个网格检查是否匹配地理位置，如果有不匹配像素，最多返回三个输入栅格数据像素位置，从0开始记。
//栅格数据全部匹配到标准网格返回true，反之返回false
bool RiskValidateTool::checkEveryGridMatching(string standardfilename, int code, string rasterfilename, 
	vector<RiskValidateToolPointI>& unmatchingPoints, string& error)
{
	std::shared_ptr<wGdalRaster> standardGridPtr(wGdalRasterFactory::OpenFile(standardfilename));
	if (standardGridPtr.get() == 0) {
        error = "打开标准网格数据文件失败";
        errorDetail = error ;
		return false;
	}

	std::shared_ptr<wGdalRaster> rasterPtr(wGdalRasterFactory::OpenFile(rasterfilename));
	if (rasterPtr.get() == 0) {
		error = "打开输入栅格数据文件失败";
        errorDetail = error ;
		return false;
	}

	int codeLow = 0;
	int codeHigh = 9;
	bool codeOk = computeXzCode(code, codeLow, codeHigh);
	if (codeOk == false) {
		error = "行政编码无效:" + wStringUtils::int2str(code);
        errorDetail = error ;
		return false;
	}

	int standxsize = standardGridPtr->getXSize();
	int standysize = standardGridPtr->getYSize();

	int rxsize = rasterPtr->getXSize();
	int rysize = rasterPtr->getYSize();

	int fillValue = rasterPtr->getNoDataValue() ;//2022-1-20


	const double* trans = rasterPtr->getTrans();
	const double* standtrans = standardGridPtr->getTrans();
	int matchCount = 0;
	int unmatchCount = 0;
	for (int iy = 0; iy < rysize; ++iy)
	{
		for (int ix = 0; ix < rxsize; ++ix)
		{
			int pxval = rasterPtr->getValuei(ix, iy, 0);
			if (pxval != fillValue)
			{
				double pixelcenterx = trans[0] + trans[1] * (ix+0.5) ;
				double pixelcentery = trans[3] + trans[5] * (iy+0.5) ;

				int standx = (pixelcenterx - standtrans[0]) / standtrans[1];
				int standy = (pixelcentery - standtrans[3]) / standtrans[5];

				bool match = false;
				if (standx >= 0 && standx < standxsize && standy >= 0 && standy < standysize)
				{
					int standval = standardGridPtr->getValuei(standx, standy, 0);
					if (standval >= codeLow && standval <= codeHigh)
					{
						++matchCount;
						match = true;
					}
				}

				if (match == false)
				{
					if( unmatchCount < 10 ){
						//避免消耗过多内存，只记录10个不匹配点
						RiskValidateToolPointI unMatchPt;
						unMatchPt.x = ix;
						unMatchPt.y = iy;
						unmatchingPoints.push_back(unMatchPt);
						++unmatchCount;
					}else{
						break ;//多了不做判断了，减少运行时间
					}
					
				}

			}
		}
	}

	if (unmatchCount > 0) {
		error = string("输入栅格数据不在标准网格中格点的数量：") + wStringUtils::int2str(unmatchingPoints.size())+ ",前三个不在标准网格中输入数据格点坐标:";
		for (int iun = 0; iun < unmatchingPoints.size(); ++iun) {
			error += "(" + wStringUtils::int2str(unmatchingPoints[iun].x) + "," + wStringUtils::int2str(unmatchingPoints[iun].y) + ") ";
        }
        errorDetail = error ;
		return false;
	}

	return true;
}

//输入行政编码计算行政编码下限和上限值
//行政编码小于0，返回false
//行政编码长度等于9，low=code，high=code
//行政编码长度小于9，自动使用0从后面补足9位，e.g. code=123456 code1=123456000 low=123456000 high=123456999
bool RiskValidateTool::computeXzCode(int code, int& codelow, int& codehigh)
{
	if (code <= 0) {
		return false;
	}

	char codeStr[32];
	sprintf(codeStr, "%d", code);
	string codeStr1(codeStr);
	if (codeStr1.length() > 9) {
		return false;
	}

	if (codeStr1.length() == 9)
	{
		codelow = code;
		codehigh = code;
	}

	if (codeStr1.length() < 9)
	{
		codelow = code;
		codehigh = code;
		int temp = 9 - codeStr1.length();
		for (int i = 0; i < temp; ++i)
		{
			codelow = codelow * 10;
			codehigh = codehigh * 10 + 9;
		}
	}
	return true;
}


//2022-1-9 对整形值的检查，是否是byte,uint16,int16,uint32,int32 危险性0-4 检查 风险0-5检查
bool RiskValidateTool::isGeoTiff_Integer(string rasterfilename,string& error) 
{
	string fileName = wStringUtils::getFileNameFromFilePath(rasterfilename);
	vector<string> strArr = wStringUtils::splitString(fileName, "_");
	if (strArr.size() < 5) {
		error = string("输入文件文件名不规范:") + fileName + ", 文件名规范为:灾害类别代码_成果类型_数据对象细化代码_6位行政区划编码_序号(01-99).xxx";
		return false;
	}

	bool pcodeOk = wStringUtils::isAllNumber( strArr[2] ) ;
	if( pcodeOk ==false ){
		error = string("输入文件文件名中数据对象细化代码无效:") +strArr[2];
		return false;
	}

	// 代码01 表示危险性区划 0-4 ， 其他应该都是风险相关 0-5
	int productCode = atof( strArr[2].c_str() ) ;
	int validMin = 1 ;//2022-1-20 无效值单独考虑，不放在这里
	int validMax = 4 ;
	if( productCode == 1 ){
		validMax = 4 ;
	}else {
		validMax = 5 ;
	}
	

	std::shared_ptr<wGdalRaster> rasterPtr(wGdalRasterFactory::OpenFile(rasterfilename));
	if (rasterPtr.get() == 0) {
		error = "打开输入栅格数据文件失败";
		return false;
	}
	int noDataVal = rasterPtr->getNoDataValue() ;//2022-1-20
	GDALDataType dataType = rasterPtr->getDataType();
	if( dataType != GDT_Byte && dataType!= GDT_UInt16 && dataType != GDT_Int16 && dataType != GDT_UInt32 && dataType!= GDT_Int32 )
	{
		//error = string("输入栅格数据的数据类型不是整形，请转换为如下类型Byte(推荐),UInt16,Int16,UInt32,Int32重新检验") ;
		error = string("输入栅格数据的数据类型不是整形");
		return false ;
	}

	
	int outOfRangePixelCount = 0 ;
	int xsize = rasterPtr->getXSize() ;
	int ysize = rasterPtr->getYSize() ;
	for(int iy = 0; iy < ysize; ++ iy )
	{
		for(int ix = 0 ; ix < xsize; ++ ix )
		{
			int pxval = rasterPtr->getValuei(ix,iy,0) ;
			if( pxval == noDataVal ) continue ;//2022-1-20
			if( pxval < validMin || pxval > validMax )
			{
				++outOfRangePixelCount;
			}

		}
	}

	if( outOfRangePixelCount>0 ){
		if( productCode==1 ){
			//危险性分级0-4
			//error = string("危险性区划（分级）数据取值范围0~4,输入数据超出范围个数:") + wStringUtils::int2str(outOfRangePixelCount) ;
			error = string("输入数据属性值不符合规范（1-4）");
		}
		else{
			//风险分级0-5
			//error = string("风险区划（分级）数据取值范围0~5,输入数据超出范围个数:") + wStringUtils::int2str(outOfRangePixelCount) ;
			error = string("输入数据属性值不符合规范（1-5）");
		}
		return false ;
	}
	return true;
}
