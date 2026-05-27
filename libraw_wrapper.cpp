#include <vector>
#include <string>
#include <stdexcept>
#include <iostream>
#include <cstring>

// Emscripten Embind
#include <emscripten/bind.h>

// LibRaw includes
#include "libraw/libraw.h"

using namespace emscripten;

class WASMLibRaw {
public:
	WASMLibRaw() {
		processor_ = new LibRaw();
	}

	~WASMLibRaw() {
		if (processor_) {
			cleanupParamsStrings();
            processor_->recycle();
			delete processor_;
			processor_ = nullptr;
		}
	}

	void open(val jsBuffer, val settings) {
		if (!processor_) {
			throw std::runtime_error("LibRaw not initialized");
		}
        // Release previous values, if any
        processor_->recycle();

		applySettings(settings);

        buffer = toNativeVector(jsBuffer);
		int ret = processor_->open_buffer((void*)buffer.data(), buffer.size());
		if (ret != LIBRAW_SUCCESS) {
			throw std::runtime_error("LibRaw: open_buffer() failed with code " + std::to_string(ret));
		}
	}

	val metadata(bool fullOutput=false) {
		if (!processor_) {
			return val::undefined();
		}

		val meta = val::object();
		// --------------------------------------------------------------------
		// 1) Basic fields: sizes, camera info, etc.
		// --------------------------------------------------------------------
		unsigned orientedWidth  = processor_->imgdata.sizes.width;
		unsigned orientedHeight = processor_->imgdata.sizes.height;
		int flipCode = processor_->imgdata.sizes.flip;  // 0..7
		if (flipCode == 5 || flipCode == 6 || flipCode == 7) {// rotations of 90/270:
			std::swap(orientedWidth, orientedHeight);
		}
		meta.set("width",       orientedWidth);
		meta.set("height",      orientedHeight);
		meta.set("raw_width",   processor_->imgdata.sizes.raw_width);
		meta.set("raw_height",  processor_->imgdata.sizes.raw_height);
		meta.set("top_margin",  processor_->imgdata.sizes.top_margin);
		meta.set("left_margin", processor_->imgdata.sizes.left_margin);

		// Basic camera info
		meta.set("camera_make",  std::string(processor_->imgdata.idata.make));
		meta.set("camera_model", std::string(processor_->imgdata.idata.model));

		// EXIF-like data
		meta.set("iso_speed",  processor_->imgdata.other.iso_speed);
		meta.set("shutter",	processor_->imgdata.other.shutter);
		meta.set("aperture",   processor_->imgdata.other.aperture);
		meta.set("focal_len",  processor_->imgdata.other.focal_len);
		meta.set("timestamp",  double(processor_->imgdata.other.timestamp));
		meta.set("shot_order", processor_->imgdata.other.shot_order);
		meta.set("desc",	   std::string(processor_->imgdata.other.desc));
		meta.set("artist",	 std::string(processor_->imgdata.other.artist));

		// GPS data
		val gpsData = val::object();

		val latitudeArr = val::array();
		for (int i = 0; i < 3; i++) {
			latitudeArr.set(i, processor_->imgdata.other.parsed_gps.latitude[i]);
		}
		gpsData.set("latitude", latitudeArr);

		val longitudeArr = val::array();
		for (int i = 0; i < 3; i++) {
			longitudeArr.set(i, processor_->imgdata.other.parsed_gps.longitude[i]);
		}
		gpsData.set("longitude", longitudeArr);
		gpsData.set("altitude", processor_->imgdata.other.parsed_gps.altitude);
			
		meta.set("gps_data", gpsData);

		// Thumbnail info
		meta.set("thumb_width",  processor_->imgdata.thumbnail.twidth);
		meta.set("thumb_height", processor_->imgdata.thumbnail.theight);
		// Cast enum to int so we don't need to register it
		meta.set("thumb_format",
				static_cast<int>(processor_->imgdata.thumbnail.tformat));

		if(fullOutput) {
			// --------------------------------------------------------------------
			// 2) Color data (imgdata.color)
			// --------------------------------------------------------------------
			const libraw_colordata_t &c = processor_->imgdata.color;
			val colorData = val::object();
			
			colorData.set("black",		 c.black);
			colorData.set("data_maximum",  c.data_maximum);
			colorData.set("maximum",	   c.maximum);
			colorData.set("fmaximum",	  c.fmaximum);
			colorData.set("fnorm",		 c.fnorm);

			{
				val camMulArr = val::array();
				for (int i = 0; i < 4; i++) {
					camMulArr.set(i, c.cam_mul[i]);
				}
				colorData.set("cam_mul", camMulArr);
			}
			{
				val preMulArr = val::array();
				for (int i = 0; i < 4; i++) {
					preMulArr.set(i, c.pre_mul[i]);
				}
				colorData.set("pre_mul", preMulArr);
			}

			// c.rgb_cam[3][4], c.cam_xyz[4][3], etc. can also be copied if needed.
			colorData.set("flash_used",   c.flash_used);
			colorData.set("canon_ev",	 c.canon_ev);
			colorData.set("model2",	   std::string(c.model2));
			colorData.set("UniqueCameraModel", std::string(c.UniqueCameraModel));
			colorData.set("LocalizedCameraModel",
						std::string(c.LocalizedCameraModel));
			colorData.set("ImageUniqueID",	  std::string(c.ImageUniqueID));
			colorData.set("RawDataUniqueID",	std::string(c.RawDataUniqueID));
			colorData.set("raw_bps",	 (int)c.raw_bps);
			colorData.set("ExifColorSpace", c.ExifColorSpace);
			// dng_color, dng_levels, etc. also here if needed

			meta.set("color_data", colorData);

			// --------------------------------------------------------------------
			// 3) Common metadata (imgdata.metadata)
			// --------------------------------------------------------------------
			const libraw_metadata_common_t &mcom = processor_->imgdata.makernotes.common;
			val metaCommon = val::object();

			metaCommon.set("FlashEC",				mcom.FlashEC);
			metaCommon.set("FlashGN",				mcom.FlashGN);
			metaCommon.set("CameraTemperature",	  mcom.CameraTemperature);
			metaCommon.set("SensorTemperature",	  mcom.SensorTemperature);
			metaCommon.set("SensorTemperature2",	 mcom.SensorTemperature2);
			metaCommon.set("LensTemperature",		mcom.LensTemperature);
			metaCommon.set("AmbientTemperature",	 mcom.AmbientTemperature);
			metaCommon.set("BatteryTemperature",	 mcom.BatteryTemperature);
			metaCommon.set("exifAmbientTemperature", mcom.exifAmbientTemperature);
			metaCommon.set("exifHumidity",		   mcom.exifHumidity);
			metaCommon.set("exifPressure",		   mcom.exifPressure);
			metaCommon.set("exifWaterDepth",		 mcom.exifWaterDepth);
			metaCommon.set("exifAcceleration",	   mcom.exifAcceleration);
			metaCommon.set("exifCameraElevationAngle", mcom.exifCameraElevationAngle);
			metaCommon.set("real_ISO", mcom.real_ISO);
			metaCommon.set("exifExposureIndex", mcom.exifExposureIndex);
			metaCommon.set("ColorSpace", (int)mcom.ColorSpace);
			metaCommon.set("firmware",   std::string(mcom.firmware));
			metaCommon.set("ExposureCalibrationShift", mcom.ExposureCalibrationShift);

			// AF info data
			{
				val afArray = val::array();
				for (int i = 0; i < mcom.afcount; i++) {
					val afItem = val::object();
					const libraw_afinfo_item_t &afdata = mcom.afdata[i];
					afItem.set("AFInfoData_tag",	 afdata.AFInfoData_tag);
					afItem.set("AFInfoData_order",   afdata.AFInfoData_order);
					afItem.set("AFInfoData_version", afdata.AFInfoData_version);
					afItem.set("AFInfoData_length",  afdata.AFInfoData_length);
					// afdata.AFInfoData is a raw pointer
					// We skip showing contents unless you want to copy them.
					afArray.set(i, afItem);
				}
				metaCommon.set("afdata", afArray);
			}

			meta.set("metadata_common", metaCommon);

			// --------------------------------------------------------------------
			// 4) MakerNotes for the brand (conditional)
			//	We store them in a property named after the brand, e.g. `canon`,
			//	`nikon`, `sony`, etc.
			// --------------------------------------------------------------------
			std::string makeStr(processor_->imgdata.idata.make);
			std::string makeLower = makeStr;
			std::transform(makeLower.begin(), makeLower.end(), makeLower.begin(), [](unsigned char c){ return std::tolower(c); });

			// ================ CANON ================
			if (makeLower.find("canon") != std::string::npos)
			{
				val canonObj = val::object();
				const libraw_canon_makernotes_t &cCanon =
					processor_->imgdata.makernotes.canon;

				canonObj.set("ColorDataVer",		cCanon.ColorDataVer);
				canonObj.set("ColorDataSubVer",	 cCanon.ColorDataSubVer);
				canonObj.set("SpecularWhiteLevel",  cCanon.SpecularWhiteLevel);
				canonObj.set("NormalWhiteLevel",	cCanon.NormalWhiteLevel);

				{
					val arr = val::array();
					for (int i = 0; i < 4; i++) {
						arr.set(i, cCanon.ChannelBlackLevel[i]);
					}
					canonObj.set("ChannelBlackLevel", arr);
				}
				canonObj.set("AverageBlackLevel", cCanon.AverageBlackLevel);

				{
					val arr = val::array();
					for (int i = 0; i < 4; i++) {
						arr.set(i, cCanon.multishot[i]);
					}
					canonObj.set("multishot", arr);
				}

				canonObj.set("MeteringMode",		  cCanon.MeteringMode);
				canonObj.set("SpotMeteringMode",	  cCanon.SpotMeteringMode);
				canonObj.set("FlashMeteringMode",	 (int)cCanon.FlashMeteringMode);
				canonObj.set("FlashExposureLock",	 cCanon.FlashExposureLock);
				canonObj.set("ExposureMode",		  cCanon.ExposureMode);
				canonObj.set("AESetting",			 cCanon.AESetting);
				canonObj.set("ImageStabilization",	cCanon.ImageStabilization);
				canonObj.set("FlashMode",			 cCanon.FlashMode);
				canonObj.set("FlashActivity",		 cCanon.FlashActivity);
				canonObj.set("FlashBits",			 cCanon.FlashBits);
				canonObj.set("ManualFlashOutput",	 cCanon.ManualFlashOutput);
				canonObj.set("FlashOutput",		   cCanon.FlashOutput);
				canonObj.set("FlashGuideNumber",	  cCanon.FlashGuideNumber);
				canonObj.set("ContinuousDrive",	   cCanon.ContinuousDrive);
				canonObj.set("SensorWidth",		   cCanon.SensorWidth);
				canonObj.set("SensorHeight",		  cCanon.SensorHeight);
				canonObj.set("AFMicroAdjMode",		cCanon.AFMicroAdjMode);
				canonObj.set("AFMicroAdjValue",	   cCanon.AFMicroAdjValue);
				canonObj.set("MakernotesFlip",		cCanon.MakernotesFlip);
				canonObj.set("RecordMode",			cCanon.RecordMode);
				canonObj.set("SRAWQuality",		   cCanon.SRAWQuality);
				canonObj.set("wbi",				   (unsigned)cCanon.wbi);
				canonObj.set("RF_lensID",			 cCanon.RF_lensID);
				canonObj.set("AutoLightingOptimizer", cCanon.AutoLightingOptimizer);
				canonObj.set("HighlightTonePriority", cCanon.HighlightTonePriority);
				canonObj.set("Quality",			   cCanon.Quality);
				canonObj.set("CanonLog",			  cCanon.CanonLog);

				
				{
					val arr = val::array();
					for (int i = 0; i < 2; i++) {
						arr.set(i, cCanon.ISOgain[i]);
					}
					canonObj.set("ISOgain", arr);
				}

				meta.set("canon", canonObj);
			}
			// ================ NIKON ================
			else if (makeLower.find("nikon") != std::string::npos)
			{
				val nikonObj = val::object();
				const libraw_nikon_makernotes_t &cNikon =
					processor_->imgdata.makernotes.nikon;

				nikonObj.set("ExposureBracketValue", cNikon.ExposureBracketValue);
				nikonObj.set("ActiveDLighting",	  cNikon.ActiveDLighting);
				nikonObj.set("ShootingMode",		 (int)cNikon.ShootingMode);

				{
					val arr = val::array();
					for (int i = 0; i < 7; i++) {
						arr.set(i, cNikon.ImageStabilization[i]);
					}
					nikonObj.set("ImageStabilization", arr);
				}

				nikonObj.set("VibrationReduction",   (int)cNikon.VibrationReduction);
				nikonObj.set("FlashSetting",		 std::string(cNikon.FlashSetting));
				nikonObj.set("FlashType",		   std::string(cNikon.FlashType));

				{
					val arr = val::array();
					for (int i = 0; i < 4; i++) {
						arr.set(i, cNikon.FlashExposureCompensation[i]);
					}
					nikonObj.set("FlashExposureCompensation", arr);
				}

				{
					val arr = val::array();
					for (int i = 0; i < 4; i++) {
						arr.set(i, cNikon.ExternalFlashExposureComp[i]);
					}
					nikonObj.set("ExternalFlashExposureComp", arr);
				}

				nikonObj.set("FlashExposureBracketValue0",
							cNikon.FlashExposureBracketValue[0]);
				nikonObj.set("FlashExposureBracketValue1",
							cNikon.FlashExposureBracketValue[1]);
				nikonObj.set("FlashExposureBracketValue2",
							cNikon.FlashExposureBracketValue[2]);
				nikonObj.set("FlashExposureBracketValue3",
							cNikon.FlashExposureBracketValue[3]);

				nikonObj.set("FlashMode",				(int)cNikon.FlashMode);
				nikonObj.set("FlashExposureCompensation2", 
													(int)cNikon.FlashExposureCompensation2);
				nikonObj.set("FlashExposureCompensation3", 
													(int)cNikon.FlashExposureCompensation3);
				nikonObj.set("FlashExposureCompensation4", 
													(int)cNikon.FlashExposureCompensation4);
				nikonObj.set("FlashSource",			 (int)cNikon.FlashSource);
				nikonObj.set("FlashFirmware0",		  (int)cNikon.FlashFirmware[0]);
				nikonObj.set("FlashFirmware1",		  (int)cNikon.FlashFirmware[1]);
				nikonObj.set("ExternalFlashFlags",	  (int)cNikon.ExternalFlashFlags);
				nikonObj.set("FlashControlCommanderMode",(int)cNikon.FlashControlCommanderMode);
				nikonObj.set("FlashOutputAndCompensation",(int)cNikon.FlashOutputAndCompensation);
				nikonObj.set("FlashFocalLength",		(int)cNikon.FlashFocalLength);
				nikonObj.set("FlashGNDistance",		 (int)cNikon.FlashGNDistance);

				{
					val arr = val::array();
					for (int i = 0; i < 4; i++) {
						arr.set(i, (int)cNikon.FlashGroupOutputAndCompensation[i]);
					}
					nikonObj.set("FlashGroupOutputAndCompensation", arr);
				}

				nikonObj.set("FlashGroupControlMode0", (int)cNikon.FlashGroupControlMode[0]);
				nikonObj.set("FlashGroupControlMode1", (int)cNikon.FlashGroupControlMode[1]);
				nikonObj.set("FlashGroupControlMode2", (int)cNikon.FlashGroupControlMode[2]);
				nikonObj.set("FlashGroupControlMode3", (int)cNikon.FlashGroupControlMode[3]);

				nikonObj.set("FlashColorFilter",	 (int)cNikon.FlashColorFilter);
				nikonObj.set("NEFCompression",	   (int)cNikon.NEFCompression);
				nikonObj.set("ExposureMode",		 cNikon.ExposureMode);
				nikonObj.set("ExposureProgram",	  cNikon.ExposureProgram);
				nikonObj.set("nMEshots",			 cNikon.nMEshots);
				nikonObj.set("MEgainOn",			 (int)cNikon.MEgainOn);

				{
					val arr = val::array();
					for (int i = 0; i < 4; i++) {
						arr.set(i, cNikon.ME_WB[i]);
					}
					nikonObj.set("ME_WB", arr);
				}

				nikonObj.set("AFFineTune",	   (int)cNikon.AFFineTune);
				nikonObj.set("AFFineTuneIndex",  (int)cNikon.AFFineTuneIndex);
				nikonObj.set("AFFineTuneAdj",	(int)cNikon.AFFineTuneAdj);
				nikonObj.set("LensDataVersion",  cNikon.LensDataVersion);
				nikonObj.set("FlashInfoVersion", cNikon.FlashInfoVersion);
				nikonObj.set("ColorBalanceVersion", cNikon.ColorBalanceVersion);
				nikonObj.set("key", (int)cNikon.key);

				{
					val arr = val::array();
					for (int i = 0; i < 4; i++) {
						arr.set(i, cNikon.NEFBitDepth[i]);
					}
					nikonObj.set("NEFBitDepth", arr);
				}

				nikonObj.set("HighSpeedCropFormat",  (int)cNikon.HighSpeedCropFormat);
				val hsc = val::object();
				hsc.set("cleft",   cNikon.SensorHighSpeedCrop.cleft);
				hsc.set("ctop",	cNikon.SensorHighSpeedCrop.ctop);
				hsc.set("cwidth",  cNikon.SensorHighSpeedCrop.cwidth);
				hsc.set("cheight", cNikon.SensorHighSpeedCrop.cheight);
				nikonObj.set("SensorHighSpeedCrop", hsc);

				nikonObj.set("SensorWidth",  (int)cNikon.SensorWidth);
				nikonObj.set("SensorHeight", (int)cNikon.SensorHeight);
				nikonObj.set("Active_D_Lighting", (int)cNikon.Active_D_Lighting);
				nikonObj.set("ShotInfoVersion",   cNikon.ShotInfoVersion);
				nikonObj.set("MakernotesFlip",	cNikon.MakernotesFlip);
				nikonObj.set("RollAngle",		 cNikon.RollAngle);
				nikonObj.set("PitchAngle",		cNikon.PitchAngle);
				nikonObj.set("YawAngle",		  cNikon.YawAngle);

				meta.set("nikon", nikonObj);
			}
			// ================ FUJI ================
			else if (makeLower.find("fuji") != std::string::npos ||
					makeLower.find("fujifilm") != std::string::npos)
			{
				val fujiObj = val::object();
				const libraw_fuji_info_t &cFuji =
					processor_->imgdata.makernotes.fuji;

				fujiObj.set("ExpoMidPointShift",		cFuji.ExpoMidPointShift);
				fujiObj.set("DynamicRange",			 cFuji.DynamicRange);
				fujiObj.set("FilmMode",				 cFuji.FilmMode);
				fujiObj.set("DynamicRangeSetting",	  cFuji.DynamicRangeSetting);
				fujiObj.set("DevelopmentDynamicRange",  cFuji.DevelopmentDynamicRange);
				fujiObj.set("AutoDynamicRange",		 cFuji.AutoDynamicRange);
				fujiObj.set("DRangePriority",		   cFuji.DRangePriority);
				fujiObj.set("DRangePriorityAuto",	   cFuji.DRangePriorityAuto);
				fujiObj.set("DRangePriorityFixed",	  cFuji.DRangePriorityFixed);
				fujiObj.set("BrightnessCompensation",   cFuji.BrightnessCompensation);
				fujiObj.set("FocusMode",				cFuji.FocusMode);
				fujiObj.set("AFMode",				   cFuji.AFMode);

				{
					val arr = val::array();
					arr.set(0, cFuji.FocusPixel[0]);
					arr.set(1, cFuji.FocusPixel[1]);
					fujiObj.set("FocusPixel", arr);
				}

				fujiObj.set("PrioritySettings",		 cFuji.PrioritySettings);
				fujiObj.set("FocusSettings",			cFuji.FocusSettings);
				fujiObj.set("AF_C_Settings",			cFuji.AF_C_Settings);
				fujiObj.set("FocusWarning",			 cFuji.FocusWarning);

				{
					val arr = val::array();
					arr.set(0, cFuji.ImageStabilization[0]);
					arr.set(1, cFuji.ImageStabilization[1]);
					arr.set(2, cFuji.ImageStabilization[2]);
					fujiObj.set("ImageStabilization", arr);
				}

				fujiObj.set("FlashMode",		cFuji.FlashMode);
				fujiObj.set("WB_Preset",		cFuji.WB_Preset);
				fujiObj.set("ShutterType",	  cFuji.ShutterType);
				fujiObj.set("ExrMode",		  cFuji.ExrMode);
				fujiObj.set("Macro",		   (int)cFuji.Macro);
				fujiObj.set("Rating",		 (int)cFuji.Rating);
				fujiObj.set("CropMode",	   (int)cFuji.CropMode);

				fujiObj.set("SerialSignature", std::string(cFuji.SerialSignature));
				fujiObj.set("SensorID",		std::string(cFuji.SensorID));
				fujiObj.set("RAFVersion",	  std::string(cFuji.RAFVersion));
				fujiObj.set("RAFDataGeneration", cFuji.RAFDataGeneration);
				fujiObj.set("RAFDataVersion",	cFuji.RAFDataVersion);
				fujiObj.set("isTSNERDTS",		cFuji.isTSNERDTS);
				fujiObj.set("DriveMode",		 (int)cFuji.DriveMode);

				{
					val arr = val::array();
					for (int i = 0; i < 9; i++) {
						arr.set(i, cFuji.BlackLevel[i]);
					}
					fujiObj.set("BlackLevel", arr);
				}

				{
					val arr = val::array();
					for (int i = 0; i < 32; i++) {
						arr.set(i, cFuji.RAFData_ImageSizeTable[i]);
					}
					fujiObj.set("RAFData_ImageSizeTable", arr);
				}
				fujiObj.set("AutoBracketing",	cFuji.AutoBracketing);
				fujiObj.set("SequenceNumber",	cFuji.SequenceNumber);
				fujiObj.set("SeriesLength",	  cFuji.SeriesLength);

				{
					val arr = val::array();
					arr.set(0, cFuji.PixelShiftOffset[0]);
					arr.set(1, cFuji.PixelShiftOffset[1]);
					fujiObj.set("PixelShiftOffset", arr);
				}

				fujiObj.set("ImageCount", cFuji.ImageCount);

				meta.set("fuji", fujiObj);
			}
			// ================ SONY ================
			else if (makeLower.find("sony") != std::string::npos)
			{
				val sonyObj = val::object();
				const libraw_sony_info_t &cSony =
					processor_->imgdata.makernotes.sony;

				sonyObj.set("CameraType",		 (int)cSony.CameraType);
				sonyObj.set("Sony0x9400_version", (int)cSony.Sony0x9400_version);
				sonyObj.set("Sony0x9400_ReleaseMode2",
							(int)cSony.Sony0x9400_ReleaseMode2);
				sonyObj.set("Sony0x9400_SequenceImageNumber",
							cSony.Sony0x9400_SequenceImageNumber);
				sonyObj.set("Sony0x9400_SequenceLength1",
							(int)cSony.Sony0x9400_SequenceLength1);
				sonyObj.set("Sony0x9400_SequenceFileNumber",
							cSony.Sony0x9400_SequenceFileNumber);
				sonyObj.set("Sony0x9400_SequenceLength2",
							(int)cSony.Sony0x9400_SequenceLength2);
				sonyObj.set("AFAreaModeSetting",	 (int)cSony.AFAreaModeSetting);
				sonyObj.set("AFAreaMode",			(int)cSony.AFAreaMode);
				{
					val arr = val::array();
					arr.set(0, cSony.FlexibleSpotPosition[0]);
					arr.set(1, cSony.FlexibleSpotPosition[1]);
					sonyObj.set("FlexibleSpotPosition", arr);
				}
				sonyObj.set("AFPointSelected",	  (int)cSony.AFPointSelected);
				sonyObj.set("AFPointSelected_0x201e",(int)cSony.AFPointSelected_0x201e);
				sonyObj.set("AFType",			   (int)cSony.AFType);

				{
					val arr = val::array();
					arr.set(0, cSony.FocusLocation[0]);
					arr.set(1, cSony.FocusLocation[1]);
					arr.set(2, cSony.FocusLocation[2]);
					arr.set(3, cSony.FocusLocation[3]);
					sonyObj.set("FocusLocation", arr);
				}

				sonyObj.set("FocusPosition",	  (int)cSony.FocusPosition);
				sonyObj.set("AFMicroAdjValue",	(int)cSony.AFMicroAdjValue);
				sonyObj.set("AFMicroAdjOn",	   (int)cSony.AFMicroAdjOn);
				sonyObj.set("AFMicroAdjRegisteredLenses",
							(int)cSony.AFMicroAdjRegisteredLenses);
				sonyObj.set("VariableLowPassFilter",
							(int)cSony.VariableLowPassFilter);
				sonyObj.set("LongExposureNoiseReduction",
							cSony.LongExposureNoiseReduction);
				sonyObj.set("HighISONoiseReduction",
							(int)cSony.HighISONoiseReduction);

				{
					val arr = val::array();
					arr.set(0, cSony.HDR[0]);
					arr.set(1, cSony.HDR[1]);
					sonyObj.set("HDR", arr);
				}

				sonyObj.set("group2010",		(int)cSony.group2010);
				sonyObj.set("group9050",		(int)cSony.group9050);
				sonyObj.set("real_iso_offset",  (int)cSony.real_iso_offset);
				sonyObj.set("MeteringMode_offset",(int)cSony.MeteringMode_offset);
				sonyObj.set("ExposureProgram_offset",(int)cSony.ExposureProgram_offset);
				sonyObj.set("ReleaseMode2_offset",(int)cSony.ReleaseMode2_offset);
				sonyObj.set("MinoltaCamID",	  cSony.MinoltaCamID);
				sonyObj.set("firmware",		  cSony.firmware);
				sonyObj.set("ImageCount3_offset",(int)cSony.ImageCount3_offset);
				sonyObj.set("ImageCount3",	   cSony.ImageCount3);
				sonyObj.set("ElectronicFrontCurtainShutter",
							cSony.ElectronicFrontCurtainShutter);
				sonyObj.set("MeteringMode2",	 (int)cSony.MeteringMode2);
				sonyObj.set("SonyDateTime",	  std::string(cSony.SonyDateTime));
				sonyObj.set("ShotNumberSincePowerUp",
							cSony.ShotNumberSincePowerUp);
				sonyObj.set("PixelShiftGroupPrefix",   cSony.PixelShiftGroupPrefix);
				sonyObj.set("PixelShiftGroupID",	   cSony.PixelShiftGroupID);
				sonyObj.set("nShotsInPixelShiftGroup", (int)cSony.nShotsInPixelShiftGroup);
				sonyObj.set("numInPixelShiftGroup",	(int)cSony.numInPixelShiftGroup);
				sonyObj.set("prd_ImageHeight",		 cSony.prd_ImageHeight);
				sonyObj.set("prd_ImageWidth",		  cSony.prd_ImageWidth);
				sonyObj.set("prd_Total_bps",		   cSony.prd_Total_bps);
				sonyObj.set("prd_Active_bps",		  cSony.prd_Active_bps);
				sonyObj.set("prd_StorageMethod",	   cSony.prd_StorageMethod);
				sonyObj.set("prd_BayerPattern",		cSony.prd_BayerPattern);
				sonyObj.set("SonyRawFileType",		 (int)cSony.SonyRawFileType);
				sonyObj.set("RAWFileType",			 (int)cSony.RAWFileType);
				sonyObj.set("RawSizeType",			 (int)cSony.RawSizeType);
				sonyObj.set("Quality",				 cSony.Quality);
				sonyObj.set("FileFormat",			  cSony.FileFormat);
				sonyObj.set("MetaVersion",			 std::string(cSony.MetaVersion));

				meta.set("sony", sonyObj);
			}
			// ================ PANASONIC ================
			else if (makeLower.find("panasonic") != std::string::npos)
			{
				val panObj = val::object();
				const libraw_panasonic_makernotes_t &cPan =
					processor_->imgdata.makernotes.panasonic;

				panObj.set("Compression",	   (int)cPan.Compression);
				panObj.set("BlackLevelDim",	 (int)cPan.BlackLevelDim);

				{
					val arr = val::array();
					for (int i = 0; i < 8; i++) {
						arr.set(i, cPan.BlackLevel[i]);
					}
					panObj.set("BlackLevel", arr);
				}

				panObj.set("Multishot",	  cPan.Multishot);
				panObj.set("gamma",		  cPan.gamma);

				{
					val arr = val::array();
					arr.set(0, cPan.HighISOMultiplier[0]);
					arr.set(1, cPan.HighISOMultiplier[1]);
					arr.set(2, cPan.HighISOMultiplier[2]);
					panObj.set("HighISOMultiplier", arr);
				}

				panObj.set("FocusStepNear",	 cPan.FocusStepNear);
				panObj.set("FocusStepCount",	cPan.FocusStepCount);
				panObj.set("ZoomPosition",	  cPan.ZoomPosition);
				panObj.set("LensManufacturer",  cPan.LensManufacturer);

				meta.set("panasonic", panObj);
			}
			// ================ OLYMPUS ================
			else if (makeLower.find("olympus") != std::string::npos)
			{
				val olyObj = val::object();
				const libraw_olympus_makernotes_t &cOly =
					processor_->imgdata.makernotes.olympus;

				{
					val arr = val::array();
					arr.set(0, cOly.CameraType2[0]);
					arr.set(1, cOly.CameraType2[1]);
					arr.set(2, cOly.CameraType2[2]);
					arr.set(3, cOly.CameraType2[3]);
					arr.set(4, cOly.CameraType2[4]);
					arr.set(5, cOly.CameraType2[5]);
					olyObj.set("CameraType2", arr);
				}
				olyObj.set("ValidBits",		(int)cOly.ValidBits);

				{
					val arr = val::array();
					arr.set(0, cOly.DriveMode[0]);
					arr.set(1, cOly.DriveMode[1]);
					arr.set(2, cOly.DriveMode[2]);
					arr.set(3, cOly.DriveMode[3]);
					arr.set(4, cOly.DriveMode[4]);
					olyObj.set("DriveMode", arr);
				}

				olyObj.set("ColorSpace",  (int)cOly.ColorSpace);
				{
					val arr = val::array();
					arr.set(0, cOly.FocusMode[0]);
					arr.set(1, cOly.FocusMode[1]);
					olyObj.set("FocusMode", arr);
				}
				olyObj.set("AutoFocus",		(int)cOly.AutoFocus);
				olyObj.set("AFPoint",		 (int)cOly.AFPoint);
				{
					val arr = val::array();
					for (int i = 0; i < 64; i++) {
						arr.set(i, cOly.AFAreas[i]);
					}
					olyObj.set("AFAreas", arr);
				}
				{
					val arr = val::array();
					arr.set(0, cOly.AFPointSelected[0]);
					arr.set(1, cOly.AFPointSelected[1]);
					olyObj.set("AFPointSelected", arr);
				}

				olyObj.set("AFResult", (int)cOly.AFResult);
				olyObj.set("AFFineTune", (int)cOly.AFFineTune);

				{
					val arr = val::array();
					arr.set(0, (int)cOly.AFFineTuneAdj[0]);
					arr.set(1, (int)cOly.AFFineTuneAdj[1]);
					arr.set(2, (int)cOly.AFFineTuneAdj[2]);
					olyObj.set("AFFineTuneAdj", arr);
				}

				// Many more fields exist in the struct if you want them all.
				olyObj.set("AspectFrameLeft",  (int)cOly.AspectFrame[0]);
				olyObj.set("AspectFrameTop",   (int)cOly.AspectFrame[1]);
				olyObj.set("AspectFrameWidth", (int)cOly.AspectFrame[2]);
				olyObj.set("AspectFrameHeight",(int)cOly.AspectFrame[3]);
				olyObj.set("Panorama_mode",	cOly.Panorama_mode);
				olyObj.set("Panorama_frameNum",cOly.Panorama_frameNum);

				meta.set("olympus", olyObj);
			}
			// ================ PENTAX ================
			else if (makeLower.find("pentax") != std::string::npos)
			{
				val pentaxObj = val::object();
				const libraw_pentax_makernotes_t &cPent =
					processor_->imgdata.makernotes.pentax;

				{
					val arr = val::array();
					arr.set(0, cPent.DriveMode[0]);
					arr.set(1, cPent.DriveMode[1]);
					arr.set(2, cPent.DriveMode[2]);
					arr.set(3, cPent.DriveMode[3]);
					pentaxObj.set("DriveMode", arr);
				}
				{
					val arr = val::array();
					arr.set(0, cPent.FocusMode[0]);
					arr.set(1, cPent.FocusMode[1]);
					pentaxObj.set("FocusMode", arr);
				}
				{
					val arr = val::array();
					arr.set(0, cPent.AFPointSelected[0]);
					arr.set(1, cPent.AFPointSelected[1]);
					pentaxObj.set("AFPointSelected", arr);
				}
				pentaxObj.set("AFPointSelected_Area", (int)cPent.AFPointSelected_Area);
				pentaxObj.set("AFPointsInFocus_version", cPent.AFPointsInFocus_version);
				pentaxObj.set("AFPointsInFocus",	   (unsigned)cPent.AFPointsInFocus);
				pentaxObj.set("FocusPosition",		 cPent.FocusPosition);
				pentaxObj.set("AFAdjustment",		  cPent.AFAdjustment);
				pentaxObj.set("AFPointMode",		   (int)cPent.AFPointMode);
				pentaxObj.set("MultiExposure",		 (int)cPent.MultiExposure);
				pentaxObj.set("Quality",			   cPent.Quality);

				meta.set("pentax", pentaxObj);
			}
			// ================ HASSELBLAD ================
			else if (makeLower.find("hasselblad") != std::string::npos)
			{
				val hassObj = val::object();
				const libraw_hasselblad_makernotes_t &cHass =
					processor_->imgdata.makernotes.hasselblad;

				hassObj.set("BaseISO",		  cHass.BaseISO);
				hassObj.set("Gain",			cHass.Gain);
				hassObj.set("Sensor",		  std::string(cHass.Sensor));
				hassObj.set("SensorUnit",	  std::string(cHass.SensorUnit));
				hassObj.set("HostBody",		std::string(cHass.HostBody));
				hassObj.set("SensorCode",	  cHass.SensorCode);
				hassObj.set("SensorSubCode",   cHass.SensorSubCode);
				hassObj.set("CoatingCode",	 cHass.CoatingCode);
				hassObj.set("uncropped",	   cHass.uncropped);
				hassObj.set("CaptureSequenceInitiator",
							std::string(cHass.CaptureSequenceInitiator));
				hassObj.set("SensorUnitConnector",
							std::string(cHass.SensorUnitConnector));
				hassObj.set("format",		  cHass.format);
				{
					val arr = val::array();
					arr.set(0, cHass.nIFD_CM[0]);
					arr.set(1, cHass.nIFD_CM[1]);
					hassObj.set("nIFD_CM", arr);
				}
				{
					val arr = val::array();
					arr.set(0, cHass.RecommendedCrop[0]);
					arr.set(1, cHass.RecommendedCrop[1]);
					hassObj.set("RecommendedCrop", arr);
				}
				{
					// 4x3 matrix
					val outer = val::array();
					for (int i = 0; i < 4; i++) {
						val row = val::array();
						for (int j = 0; j < 3; j++) {
							row.set(j, cHass.mnColorMatrix[i][j]);
						}
						outer.set(i, row);
					}
					hassObj.set("mnColorMatrix", outer);
				}

				meta.set("hasselblad", hassObj);
			}
			// ================ RICOH ================
			else if (makeLower.find("ricoh") != std::string::npos)
			{
				val ricohObj = val::object();
				const libraw_ricoh_makernotes_t &cRicoh =
					processor_->imgdata.makernotes.ricoh;

				ricohObj.set("AFStatus",		 cRicoh.AFStatus);

				{
					val arrX = val::array();
					arrX.set(0, cRicoh.AFAreaXPosition[0]);
					arrX.set(1, cRicoh.AFAreaXPosition[1]);
					ricohObj.set("AFAreaXPosition", arrX);
				}
				{
					val arrY = val::array();
					arrY.set(0, cRicoh.AFAreaYPosition[0]);
					arrY.set(1, cRicoh.AFAreaYPosition[1]);
					ricohObj.set("AFAreaYPosition", arrY);
				}
				ricohObj.set("AFAreaMode",		 (int)cRicoh.AFAreaMode);
				ricohObj.set("SensorWidth",		cRicoh.SensorWidth);
				ricohObj.set("SensorHeight",	   cRicoh.SensorHeight);
				ricohObj.set("CroppedImageWidth",  cRicoh.CroppedImageWidth);
				ricohObj.set("CroppedImageHeight", cRicoh.CroppedImageHeight);
				ricohObj.set("WideAdapter",		cRicoh.WideAdapter);
				ricohObj.set("CropMode",		   cRicoh.CropMode);
				ricohObj.set("NDFilter",		   cRicoh.NDFilter);
				ricohObj.set("AutoBracketing",	 cRicoh.AutoBracketing);
				ricohObj.set("MacroMode",		  cRicoh.MacroMode);
				ricohObj.set("FlashMode",		  cRicoh.FlashMode);
				ricohObj.set("FlashExposureComp",  cRicoh.FlashExposureComp);
				ricohObj.set("ManualFlashOutput",  cRicoh.ManualFlashOutput);

				meta.set("ricoh", ricohObj);
			}
			// ================ SAMSUNG ================
			else if (makeLower.find("samsung") != std::string::npos)
			{
				val samsungObj = val::object();
				const libraw_samsung_makernotes_t &cSamsung =
					processor_->imgdata.makernotes.samsung;

				{
					val arr = val::array();
					arr.set(0, cSamsung.ImageSizeFull[0]);
					arr.set(1, cSamsung.ImageSizeFull[1]);
					arr.set(2, cSamsung.ImageSizeFull[2]);
					arr.set(3, cSamsung.ImageSizeFull[3]);
					samsungObj.set("ImageSizeFull", arr);
				}
				{
					val arr = val::array();
					arr.set(0, cSamsung.ImageSizeCrop[0]);
					arr.set(1, cSamsung.ImageSizeCrop[1]);
					arr.set(2, cSamsung.ImageSizeCrop[2]);
					arr.set(3, cSamsung.ImageSizeCrop[3]);
					samsungObj.set("ImageSizeCrop", arr);
				}

				{
					val arr = val::array();
					for (int i = 0; i < 11; i++) {
						arr.set(i, cSamsung.key[i]);
					}
					samsungObj.set("key", arr);
				}

				samsungObj.set("ColorSpace0", cSamsung.ColorSpace[0]);
				samsungObj.set("ColorSpace1", cSamsung.ColorSpace[1]);

				samsungObj.set("DigitalGain", cSamsung.DigitalGain);
				samsungObj.set("DeviceType",  cSamsung.DeviceType);
				samsungObj.set("LensFirmware", std::string(cSamsung.LensFirmware));

				meta.set("samsung", samsungObj);
			}
			// ================ KODAK ================
			else if (makeLower.find("kodak") != std::string::npos)
			{
				val kodakObj = val::object();
				const libraw_kodak_makernotes_t &cKodak =
					processor_->imgdata.makernotes.kodak;

				kodakObj.set("BlackLevelTop",	  cKodak.BlackLevelTop);
				kodakObj.set("BlackLevelBottom",   cKodak.BlackLevelBottom);
				kodakObj.set("offset_left",		cKodak.offset_left);
				kodakObj.set("offset_top",		 cKodak.offset_top);
				kodakObj.set("clipBlack",		  cKodak.clipBlack);
				kodakObj.set("clipWhite",		  cKodak.clipWhite);

				kodakObj.set("val018percent", cKodak.val018percent);
				kodakObj.set("val100percent", cKodak.val100percent);
				kodakObj.set("val170percent", cKodak.val170percent);
				kodakObj.set("MakerNoteKodak8a", cKodak.MakerNoteKodak8a);
				kodakObj.set("ISOCalibrationGain", cKodak.ISOCalibrationGain);
				kodakObj.set("AnalogISO", cKodak.AnalogISO);

				meta.set("kodak", kodakObj);
			}
			// ================ PHASE ONE (p1) ================
			else if (makeLower.find("phase one") != std::string::npos)
			{
				val p1Obj = val::object();
				const libraw_p1_makernotes_t &cP1 = processor_->imgdata.makernotes.phaseone;

				p1Obj.set("Software",	   std::string(cP1.Software));
				p1Obj.set("SystemType",	 std::string(cP1.SystemType));
				p1Obj.set("FirmwareString", std::string(cP1.FirmwareString));
				p1Obj.set("SystemModel",	std::string(cP1.SystemModel));

				meta.set("p1", p1Obj);
			}	
		}
		return meta;
	}

	val imageData() {
		if (!processor_) {
			return val::undefined();
		}

		// If not yet unpacked/processed, do it now
		if (!isUnpacked) {
			isUnpacked = true;

			int ret = processor_->unpack();
			if (ret != LIBRAW_SUCCESS) {
				throw std::runtime_error("LibRaw: unpack() failed with code " + std::to_string(ret));
			}

			ret = processor_->dcraw_process();
			if (ret != LIBRAW_SUCCESS) {
				throw std::runtime_error("LibRaw: dcraw_process() failed with code " + std::to_string(ret));
			}
		}

		// Make a processed image in memory
		libraw_processed_image_t* out = nullptr;
		out = processor_->dcraw_make_mem_image();
		if (!out) {
			return val::undefined();
		}

		// Prepare a JS object to hold all the result fields
		val resultObj = val::object();

		// Store the basic image info
		resultObj.set("height", out->height);
		resultObj.set("width",  out->width);
		resultObj.set("colors", out->colors);
		resultObj.set("bits",   out->bits);
        resultObj.set("dataSize", (unsigned int)out->data_size);
        resultObj.set("data", toJSTypedArray(out->bits, out->data_size, out->data));

		processor_->dcraw_clear_mem(out);

		return resultObj;
	}
    
    val thumbnailData() {
		if (!processor_) return val::undefined();

        // Call LibRaw's unpack_thumb function
        int ret = processor_->unpack_thumb();

        if (ret != LIBRAW_SUCCESS) return val::undefined();

        // Get thumbnail data structure
        libraw_processed_image_t *img = processor_->dcraw_make_mem_thumb();
        
        if (!img) return val::undefined();

        val resultObj = val::object();

        resultObj.set("data", toJSTypedArray(8, img->data_size, img->data));
        resultObj.set("width", img->width);
        resultObj.set("height", img->height);
		
        std::string formatStr = "unknown";
        if (img->type == LIBRAW_IMAGE_JPEG) formatStr = "jpeg";
        else if (img->type == LIBRAW_IMAGE_BITMAP) formatStr = "bitmap";
        
        resultObj.set("format", formatStr);
        
        LibRaw::dcraw_clear_mem(img);

        return resultObj;
    }

private:
	LibRaw* processor_ = nullptr;
    std::vector<uint8_t> buffer;
	bool isUnpacked = false;

	void applySettings(const val& settings) {
		// If 'settings' is null or undefined, just skip
		if (settings.isNull() || settings.isUndefined()) {
			return;
		}

		if (settings.typeOf().as<std::string>() != "object") {
			return;
		}

		libraw_output_params_t &params = processor_->imgdata.params;

		// -- ARRAYS --
		if (settings.hasOwnProperty("greybox") && !settings["greybox"].isNull() && !settings["greybox"].isUndefined()) {
			val arr = settings["greybox"];
			if (arr["length"].as<unsigned>() == 4) {
				for (int i = 0; i < 4; i++) {
					params.greybox[i] = arr[i].as<unsigned>();
				}
			}
		}
		if (settings.hasOwnProperty("cropbox") && !settings["cropbox"].isNull() && !settings["cropbox"].isUndefined()) {
			val arr = settings["cropbox"];
			if (arr["length"].as<unsigned>() == 4) {
				for (int i = 0; i < 4; i++) {
					params.cropbox[i] = arr[i].as<unsigned>();
				}
			}
		}
		if (settings.hasOwnProperty("aber") && !settings["aber"].isNull() && !settings["aber"].isUndefined()) {
			val arr = settings["aber"];
			if (arr["length"].as<unsigned>() == 4) {
				for (int i = 0; i < 4; i++) {
					params.aber[i] = arr[i].as<double>();
				}
			}
		}
		if (settings.hasOwnProperty("gamm") && !settings["gamm"].isNull() && !settings["gamm"].isUndefined()) {
			val arr = settings["gamm"];
			if (arr["length"].as<unsigned>() == 6) {
				for (int i = 0; i < 6; i++) {
					params.gamm[i] = arr[i].as<double>();
				}
			}
		}
		if (settings.hasOwnProperty("userMul") && !settings["userMul"].isNull() && !settings["userMul"].isUndefined()) {
			val arr = settings["userMul"];
			if (arr["length"].as<unsigned>() == 4) {
				for (int i = 0; i < 4; i++) {
					params.user_mul[i] = arr[i].as<float>();
				}
			}
		}

		// -- FLOATS --
		if (settings.hasOwnProperty("bright")) {
			params.bright = settings["bright"].as<float>();
		}
		if (settings.hasOwnProperty("threshold")) {
			params.threshold = settings["threshold"].as<float>();
		}
		if (settings.hasOwnProperty("autoBrightThr")) {
			params.auto_bright_thr = settings["autoBrightThr"].as<float>();
		}
		if (settings.hasOwnProperty("adjustMaximumThr")) {
			params.adjust_maximum_thr = settings["adjustMaximumThr"].as<float>();
		}
		if (settings.hasOwnProperty("expShift")) {
			params.exp_shift = settings["expShift"].as<float>();
		}
		if (settings.hasOwnProperty("expPreser")) {
			params.exp_preser = settings["expPreser"].as<float>();
		}

		// -- INTEGERS --
		if (settings.hasOwnProperty("halfSize")) {
			params.half_size = settings["halfSize"].as<int>();
		}
		if (settings.hasOwnProperty("fourColorRgb")) {
			params.four_color_rgb = settings["fourColorRgb"].as<int>();
		}
		if (settings.hasOwnProperty("highlight")) {
			params.highlight = settings["highlight"].as<int>();
		}
		if (settings.hasOwnProperty("useAutoWb")) {
			params.use_auto_wb = settings["useAutoWb"].as<int>();
		}
		if (settings.hasOwnProperty("useCameraWb")) {
			params.use_camera_wb = settings["useCameraWb"].as<int>();
		}
		if (settings.hasOwnProperty("useCameraMatrix")) {
			params.use_camera_matrix = settings["useCameraMatrix"].as<int>();
		}
		if (settings.hasOwnProperty("outputColor")) {
			params.output_color = settings["outputColor"].as<int>();
		}
		if (settings.hasOwnProperty("outputBps")) {
			params.output_bps = settings["outputBps"].as<int>();
		}
		if (settings.hasOwnProperty("outputTiff")) {
			params.output_tiff = settings["outputTiff"].as<int>();
		}
		if (settings.hasOwnProperty("outputFlags")) {
			params.output_flags = settings["outputFlags"].as<int>();
		}
		if (settings.hasOwnProperty("userFlip")) {
			params.user_flip = settings["userFlip"].as<int>();
		}
		if (settings.hasOwnProperty("userQual")) {
			params.user_qual = settings["userQual"].as<int>();
		}
		if (settings.hasOwnProperty("userBlack")) {
			params.user_black = settings["userBlack"].as<int>();
		}
		if (settings.hasOwnProperty("userCblack")) {
			val arr = settings["userCblack"];
			if (arr["length"].as<unsigned>() == 4) {
				for (int i = 0; i < 4; i++) {
					params.user_cblack[i] = arr[i].as<int>();
				}
			}
		}
		if (settings.hasOwnProperty("userSat")) {
			params.user_sat = settings["userSat"].as<int>();
		}
		if (settings.hasOwnProperty("medPasses")) {
			params.med_passes = settings["medPasses"].as<int>();
		}
		if (settings.hasOwnProperty("noAutoBright")) {
			params.no_auto_bright = settings["noAutoBright"].as<int>();
		}
		if (settings.hasOwnProperty("useFujiRotate")) {
			params.use_fuji_rotate = settings["useFujiRotate"].as<int>();
		}
		if (settings.hasOwnProperty("greenMatching")) {
			params.green_matching = settings["greenMatching"].as<int>();
		}
		if (settings.hasOwnProperty("dcbIterations")) {
			params.dcb_iterations = settings["dcbIterations"].as<int>();
		}
		if (settings.hasOwnProperty("dcbEnhanceFl")) {
			params.dcb_enhance_fl = settings["dcbEnhanceFl"].as<int>();
		}
		if (settings.hasOwnProperty("fbddNoiserd")) {
			params.fbdd_noiserd = settings["fbddNoiserd"].as<int>();
		}
		if (settings.hasOwnProperty("expCorrec")) {
			params.exp_correc = settings["expCorrec"].as<int>();
		}
		if (settings.hasOwnProperty("noAutoScale")) {
			params.no_auto_scale = settings["noAutoScale"].as<int>();
		}
		if (settings.hasOwnProperty("noInterpolation")) {
			params.no_interpolation = settings["noInterpolation"].as<int>();
		}

		// -- STRINGS (C-strings) --
		if (settings.hasOwnProperty("outputProfile") && settings["outputProfile"].typeOf().as<std::string>()=="string") {
			setStringMember(params.output_profile, settings["outputProfile"].as<std::string>());
		}
		if (settings.hasOwnProperty("cameraProfile") && settings["cameraProfile"].typeOf().as<std::string>()=="string") {
			setStringMember(params.camera_profile, settings["cameraProfile"].as<std::string>());
		}
		if (settings.hasOwnProperty("badPixels") && settings["badPixels"].typeOf().as<std::string>()=="string") {
			setStringMember(params.bad_pixels, settings["badPixels"].as<std::string>());
		}
		if (settings.hasOwnProperty("darkFrame") && settings["darkFrame"].typeOf().as<std::string>()=="string") {
			setStringMember(params.dark_frame, settings["darkFrame"].as<std::string>());
		}
	}
	// Convert a JS Uint8Array to a std::vector<uint8_t>
	std::vector<uint8_t> toNativeVector(const val &jsBufLike) {
        const val Uint8Array = val::global("Uint8Array");
        const val ArrayBuffer = val::global("ArrayBuffer");

        // Normalize to a Uint8Array, whether the input is ArrayBuffer or any ArrayBufferView
        val u8 = jsBufLike.instanceof(Uint8Array)
                 ? jsBufLike
                 : (jsBufLike.instanceof(ArrayBuffer)
                      ? Uint8Array.new_(jsBufLike)
                      : Uint8Array.new_(jsBufLike["buffer"],
                                        jsBufLike["byteOffset"],
                                        jsBufLike["byteLength"]));

        const size_t n = u8["byteLength"].as<size_t>();
        std::vector<uint8_t> out(n);

        // Create a Uint8Array view into WASM memory and copy JS -> WASM in one go
        val wasmView = val(emscripten::typed_memory_view(out.size(), out.data()));
        wasmView.call<void>("set", u8);   // single memcpy under the hood

        return out;
	}
    
    val toJSTypedArray(size_t bits, size_t data_size, uint8_t *data) {
        if (bits == 16) {
            unsigned length = (unsigned)data_size / 2;
            val typedArrayCtor = val::global("Uint16Array");
            val typedArray = typedArrayCtor.new_(val(length));
            val memView = val(typed_memory_view(length, (uint16_t*)data));
            typedArray.call<void>("set", memView);
            return typedArray;
        } else {
            val typedArrayCtor = val::global("Uint8Array");
            val typedArray = typedArrayCtor.new_(val((unsigned)data_size));
            val memView = val(typed_memory_view(data_size, (uint8_t*)data));
            typedArray.call<void>("set", memView);
            return typedArray;
        }
    }
    
	void setStringMember(char*& dest, const std::string& value) {
		if (dest) {
			delete[] dest;
			dest = nullptr;
		}
		if (!value.empty()) {
			dest = new char[value.size() + 1];
			std::strcpy(dest, value.c_str());
		}
	}

	void cleanupParamsStrings() {
		libraw_output_params_t &params = processor_->imgdata.params;

		if (params.output_profile) {
			delete[] params.output_profile;
			params.output_profile = nullptr;
		}
		if (params.camera_profile) {
			delete[] params.camera_profile;
			params.camera_profile = nullptr;
		}
		if (params.bad_pixels) {
			delete[] params.bad_pixels;
			params.bad_pixels = nullptr;
		}
		if (params.dark_frame) {
			delete[] params.dark_frame;
			params.dark_frame = nullptr;
		}
	}
};

EMSCRIPTEN_BINDINGS(libraw_module) {
	register_vector<uint8_t>("VectorUint8");
	class_<WASMLibRaw>("LibRaw")
		.constructor<>()
		.function("open", &WASMLibRaw::open)
		.function("metadata", &WASMLibRaw::metadata)
        .function("imageData", &WASMLibRaw::imageData)
		.function("thumbnailData", &WASMLibRaw::thumbnailData);
}
