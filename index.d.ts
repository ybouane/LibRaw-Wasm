export type RawPixelData = Uint8Array | Uint16Array;

/**
 * Settings used when decoding a RAW image.
 *
 * The package exposes an asynchronous API and processes decoding in a Web Worker,
 * which helps avoid blocking the main UI thread.
 */
export interface LibRawSettings {
  /** Brightness, mapped to `-b <float>`. */
  bright?: number;
  /** Wavelet denoise threshold, mapped to `-n <float>`. */
  threshold?: number;
  /** Portion of clipped pixels used for auto-brightening. */
  autoBrightThr?: number;
  /** Auto-adjust max if channel overflow is above the threshold. */
  adjustMaximumThr?: number;
  /** Exposure shift in linear scale; requires `expCorrec = true`. */
  expShift?: number;
  /** Preserve highlights when `expShift > 1` (`0..1`). */
  expPreser?: number;

  /** Output at half size, mapped to `-h`. */
  halfSize?: boolean;
  /** Separate interpolation for the two green channels, mapped to `-f`. */
  fourColorRgb?: boolean;
  /** Highlight mode, mapped to `-H` (`0..9`). */
  highlight?: number;
  /** Use auto white balance, mapped to `-a`. */
  useAutoWb?: boolean;
  /** Use the camera's recorded white balance, mapped to `-w`. */
  useCameraWb?: boolean;
  /** Color profile usage, mapped to `+M/-M` (`0=off, 1=on if WB, 3=always`). */
  useCameraMatrix?: number;
  /** Output colorspace, mapped to `-o` (`0..8`). */
  outputColor?: number;
  /** Output bits per sample, mapped to `-4` (`8` or `16`). */
  outputBps?: number;
  /** Output TIFF if `true`, otherwise PPM, mapped to `-T`. */
  outputTiff?: boolean;
  /** Bitfield for custom output flags. */
  outputFlags?: number;
  /** Flip or rotate output, mapped to `-t` (`0..7`, `-1` uses RAW value). */
  userFlip?: number;
  /** Interpolation quality, mapped to `-q` (`0..12`). */
  userQual?: number;
  /** User black level, mapped to `-k`. */
  userBlack?: number;
  /** Per-channel black offsets: red, green, blue, green2. */
  userCblack?: [number, number, number, number];
  /** Saturation level, mapped to `-S`. */
  userSat?: number;
  /** Median filter passes, mapped to `-m`. */
  medPasses?: number;
  /** Disable auto brightness, mapped to `-W`. */
  noAutoBright?: boolean;
  /** Fuji sensor rotation, mapped to `-j` (`-1` use, `0` off, `1` on). */
  useFujiRotate?: number;
  /** Fix green channel imbalance. */
  greenMatching?: boolean;
  /** Additional DCB passes (`-1` disables). */
  dcbIterations?: number;
  /** Enhance color fidelity in DCB. */
  dcbEnhanceFl?: boolean;
  /** FBDD denoise strength (`0` off, `1` light, `2` full). */
  fbddNoiserd?: number;
  /** Enable exposure correction so `expShift` and `expPreser` apply. */
  expCorrec?: boolean;
  /** Skip `scale_colors`, which affects white balance. */
  noAutoScale?: boolean;
  /** Skip demosaic entirely and output the raw mosaic. */
  noInterpolation?: boolean;

  /** WB calculation rectangle: `x, y, width, height`. */
  greybox?: [number, number, number, number] | null;
  /** Cropping rectangle: `left, top, width, height`, applied before rotation. */
  cropbox?: [number, number, number, number] | null;
  /** Red and blue channel multipliers. */
  aber?: [number, number, number] | null;
  /** Tone curve values: `power`, `toe_slope`. */
  gamm?: [number, number] | null;
  /** User white-balance multipliers: red, green, blue, green2. */
  userMul?: [number, number, number, number] | null;

  /** Output ICC profile filename, if compiled with LCMS. */
  outputProfile?: string | null;
  /** Camera ICC profile filename, or `embed`. */
  cameraProfile?: string | null;
  /** Bad-pixels map filename. */
  badPixels?: string | null;
  /** Dark-frame filename, expected to be a 16-bit PGM. */
  darkFrame?: string | null;
}

/**
 * GPS coordinates returned by the metadata wrapper.
 *
 * The `latitude` and `longitude` properties are 3-number tuples representing
 * degrees, minutes and seconds: `[degrees, minutes, seconds]`.
 * - `degrees`: integer degrees (latitude 0..90, longitude 0..180)
 * - `minutes`: integer minutes (0..59)
 * - `seconds`: seconds and fractional seconds (may be a float, e.g. 40.12)
 *
 * Example: `[51, 28, 40.12]` === 51°28'40.12".
 *
 * Note: The EXIF/GPS hemisphere (N/S, E/W) may be provided separately in
 * original EXIF fields; consult the raw file's GPSRef if you require an
 * explicit hemisphere. `altitude` is expressed in metres.
 */
export interface GpsData {
  latitude: [number, number, number];
  longitude: [number, number, number];
  altitude: number;
}

/**
 * Minimal AF item shape exposed through `metadata_common.afdata`.
 */
export interface AfInfoDataItem {
  AFInfoData_tag: number;
  AFInfoData_order: number;
  AFInfoData_version: number;
  AFInfoData_length: number;
}

/**
 * Shared metadata block emitted by the wrapper.
 */
export interface MetadataCommon {
  /** Flash exposure compensation (EC): adjustment to flash output level. */
  FlashEC?: number;
  /** Flash guide number (GN): indicates flash power rating (typically for ISO 100). */
  FlashGN?: number;
  CameraTemperature?: number;
  SensorTemperature?: number;
  SensorTemperature2?: number;
  LensTemperature?: number;
  AmbientTemperature?: number;
  BatteryTemperature?: number;
  exifAmbientTemperature?: number;
  exifHumidity?: number;
  exifPressure?: number;
  exifWaterDepth?: number;
  exifAcceleration?: number;
  exifCameraElevationAngle?: number;
  real_ISO?: number;
  exifExposureIndex?: number;
  ColorSpace?: number;
  firmware?: string;
  ExposureCalibrationShift?: number;
  afdata?: AfInfoDataItem[];
  [key: string]: unknown;
}

export type Vec2 = [number, number];
export type Vec3 = [number, number, number];
export type Vec4 = [number, number, number, number];
export type Vec5 = [number, number, number, number, number];
export type Vec6 = [number, number, number, number, number, number];
export type Vec7 = [number, number, number, number, number, number, number];
export type Vec8 = [number, number, number, number, number, number, number, number];
export type Vec9 = [number, number, number, number, number, number, number, number, number];
export type Vec11 = [number, number, number, number, number, number, number, number, number, number, number];
export type Matrix4x3 = [Vec3, Vec3, Vec3, Vec3];

export interface CanonMetadata {
  ColorDataVer?: number;
  ColorDataSubVer?: number;
  SpecularWhiteLevel?: number;
  NormalWhiteLevel?: number;
  ChannelBlackLevel?: Vec4;
  AverageBlackLevel?: number;
  multishot?: Vec4;
  MeteringMode?: number;
  SpotMeteringMode?: number;
  FlashMeteringMode?: number;
  FlashExposureLock?: number;
  ExposureMode?: number;
  AESetting?: number;
  ImageStabilization?: number;
  FlashMode?: number;
  FlashActivity?: number;
  FlashBits?: number;
  ManualFlashOutput?: number;
  FlashOutput?: number;
  FlashGuideNumber?: number;
  ContinuousDrive?: number;
  SensorWidth?: number;
  SensorHeight?: number;
  AFMicroAdjMode?: number;
  AFMicroAdjValue?: number;
  MakernotesFlip?: number;
  RecordMode?: number;
  SRAWQuality?: number;
  wbi?: number;
  RF_lensID?: number;
  AutoLightingOptimizer?: number;
  HighlightTonePriority?: number;
  Quality?: number;
  CanonLog?: number;
  ISOgain?: Vec2;
}

export interface NikonSensorHighSpeedCrop {
  cleft?: number;
  ctop?: number;
  cwidth?: number;
  cheight?: number;
}

export interface NikonMetadata {
  NEFBitDepth?: Vec4;
  HighSpeedCropFormat?: number;
  SensorHighSpeedCrop?: NikonSensorHighSpeedCrop;
  SensorWidth?: number;
  SensorHeight?: number;
  Active_D_Lighting?: number;
  ShotInfoVersion?: number;
  MakernotesFlip?: number;
  RollAngle?: number;
  PitchAngle?: number;
  YawAngle?: number;
  FlashControlCommanderMode?: number;
  FlashOutputAndCompensation?: number;
  FlashFocalLength?: number;
  FlashGNDistance?: number;
  FlashGroupOutputAndCompensation?: Vec4;
  FlashGroupControlMode0?: number;
  FlashGroupControlMode1?: number;
  FlashGroupControlMode2?: number;
  FlashGroupControlMode3?: number;
  FlashColorFilter?: number;
  NEFCompression?: number;
  ExposureMode?: number;
  ExposureProgram?: number;
  nMEshots?: number;
  MEgainOn?: number;
  ME_WB?: Vec4;
  AFFineTune?: number;
  AFFineTuneIndex?: number;
  AFFineTuneAdj?: number;
  LensDataVersion?: number | string;
  FlashInfoVersion?: number | string;
  ColorBalanceVersion?: number | string;
  key?: number;
  ExposureBracketValue?: number;
  ActiveDLighting?: number;
  ShootingMode?: number;
  ImageStabilization?: Vec7;
  VibrationReduction?: number;
  FlashSetting?: string;
  FlashType?: string;
  FlashExposureCompensation?: number[];
  ExternalFlashExposureComp?: Vec4;
  FlashExposureBracketValue0?: number;
  FlashExposureBracketValue1?: number;
  FlashExposureBracketValue2?: number;
  FlashExposureBracketValue3?: number;
}

export interface FujiMetadata {
  ExpoMidPointShift?: number;
  DynamicRange?: number;
  FilmMode?: number;
  DynamicRangeSetting?: number;
  DevelopmentDynamicRange?: number;
  AutoDynamicRange?: number;
  DRangePriority?: number;
  DRangePriorityAuto?: number;
  DRangePriorityFixed?: number;
  ImageStabilization?: Vec3;
  FlashMode?: number;
  WB_Preset?: number;
  ShutterType?: number;
  ExrMode?: number;
  Macro?: number;
  Rating?: number;
  CropMode?: number;
  SerialSignature?: string;
  SensorID?: string;
  RAFVersion?: string;
  RAFDataGeneration?: number;
  RAFDataVersion?: number;
  isTSNERDTS?: number;
  DriveMode?: number;
  BlackLevel?: Vec9;
  RAFData_ImageSizeTable?: number[];
  AutoBracketing?: number;
  SequenceNumber?: number;
  SeriesLength?: number;
  PixelShiftOffset?: Vec2;
  ImageCount?: number;
  BrightnessCompensation?: number;
  FocusMode?: number;
  AFMode?: number;
  FocusPixel?: Vec2;
  PrioritySettings?: number;
  FocusSettings?: number;
  AF_C_Settings?: number;
  FocusWarning?: number;
}

export interface SonyMetadata {
  CameraType?: number;
  Sony0x9400_version?: number;
  Sony0x9400_ReleaseMode2?: number;
  Sony0x9400_SequenceImageNumber?: number;
  Sony0x9400_SequenceLength1?: number;
  Sony0x9400_SequenceFileNumber?: number;
  Sony0x9400_SequenceLength2?: number;
  AFAreaModeSetting?: number;
  AFAreaMode?: number;
  FlexibleSpotPosition?: Vec2;
  AFPointSelected?: number;
  AFPointSelected_0x201e?: number;
  AFType?: number;
  FocusLocation?: Vec4;
  FocusPosition?: number;
  AFMicroAdjValue?: number;
  AFMicroAdjOn?: number;
  AFMicroAdjRegisteredLenses?: number;
  VariableLowPassFilter?: number;
  LongExposureNoiseReduction?: number;
  HighISONoiseReduction?: number;
  numInPixelShiftGroup?: number;
  prd_ImageHeight?: number;
  prd_ImageWidth?: number;
  prd_Total_bps?: number;
  prd_Active_bps?: number;
  prd_StorageMethod?: number;
  prd_BayerPattern?: number;
  SonyRawFileType?: number;
  RAWFileType?: number;
  RawSizeType?: number;
  ImageCount3_offset?: number;
  ImageCount3?: number;
  ElectronicFrontCurtainShutter?: number;
  MeteringMode2?: number;
  SonyDateTime?: string;
  ShotNumberSincePowerUp?: number;
  PixelShiftGroupPrefix?: string;
  PixelShiftGroupID?: string;
  nShotsInPixelShiftGroup?: number;
  HDR?: Vec2;
  group2010?: number;
  group9050?: number;
  real_iso_offset?: number;
  MeteringMode_offset?: number;
  ExposureProgram_offset?: number;
  ReleaseMode2_offset?: number;
  MinoltaCamID?: number;
  firmware?: string;
  Quality?: number;
  FileFormat?: number;
  MetaVersion?: string;
}

export interface PanasonicMetadata {
  Compression?: number;
  BlackLevelDim?: number;
  BlackLevel?: Vec8;
  Multishot?: number;
  gamma?: number;
  HighISOMultiplier?: Vec3;
  FocusStepNear?: number;
  FocusStepCount?: number;
  ZoomPosition?: number;
  LensManufacturer?: number;
}

export interface OlympusMetadata {
  CameraType2?: Vec6;
  ValidBits?: number;
  DriveMode?: Vec5;
  ColorSpace?: number;
  FocusMode?: Vec2;
  AutoFocus?: number;
  AFPoint?: number;
  AFAreas?: number[];
  AFPointSelected?: Vec2;
  AFResult?: number;
  AFFineTune?: number;
  AFFineTuneAdj?: Vec3;
  AspectFrameLeft?: number;
  AspectFrameTop?: number;
  AspectFrameWidth?: number;
  AspectFrameHeight?: number;
  Panorama_mode?: number;
  Panorama_frameNum?: number;
}

export interface PentaxMetadata {
  DriveMode?: Vec4;
  FocusMode?: Vec2;
  AFPointSelected?: Vec2;
  AFPointSelected_Area?: number;
  AFPointsInFocus_version?: number;
  AFPointsInFocus?: number;
  FocusPosition?: number;
  AFAdjustment?: number;
  AFPointMode?: number;
  MultiExposure?: number;
  Quality?: number;
}

export interface HasselbladMetadata {
  BaseISO?: number;
  Gain?: number;
  Sensor?: string;
  SensorUnit?: string;
  HostBody?: string;
  SensorCode?: number;
  SensorSubCode?: number;
  CoatingCode?: number;
  uncropped?: number;
  CaptureSequenceInitiator?: string;
  SensorUnitConnector?: string;
  format?: number;
  nIFD_CM?: Vec2;
  RecommendedCrop?: Vec2;
  mnColorMatrix?: Matrix4x3;
}

export interface RicohMetadata {
  AFStatus?: number;
  AFAreaXPosition?: Vec2;
  AFAreaYPosition?: Vec2;
  AFAreaMode?: number;
  SensorWidth?: number;
  SensorHeight?: number;
  CroppedImageWidth?: number;
  CroppedImageHeight?: number;
  WideAdapter?: number;
  CropMode?: number;
  NDFilter?: number;
  AutoBracketing?: number;
  MacroMode?: number;
  FlashMode?: number;
  FlashExposureComp?: number;
  ManualFlashOutput?: number;
}

export interface KodakMetadata {
  BlackLevelTop?: number;
  BlackLevelBottom?: number;
  offset_left?: number;
  offset_top?: number;
  clipBlack?: number;
  clipWhite?: number;
  val018percent?: number;
  val100percent?: number;
  val170percent?: number;
  MakerNoteKodak8a?: number;
  ISOCalibrationGain?: number;
  AnalogISO?: number;
}

export interface PhaseOneMetadata {
  Software?: string;
  SystemType?: string;
  FirmwareString?: string;
  SystemModel?: string;
}

export interface SamsungMetadata {
  ImageSizeFull?: Vec4;
  ImageSizeCrop?: Vec4;
  key?: Vec11;
  ColorSpace0?: number;
  ColorSpace1?: number;
  DigitalGain?: number;
  DeviceType?: number;
  LensFirmware?: string;
}

/**
 * Decoded metadata returned by the wrapper.
 */
export interface Metadata {
  width: number;
  height: number;
  raw_width: number;
  raw_height: number;
  top_margin: number;
  left_margin: number;
  camera_make: string;
  camera_model: string;
  iso_speed: number;
  shutter: number;
  aperture: number;
  focal_len: number;
  timestamp: Date;
  shot_order: number;
  desc: string;
  artist: string;
  thumb_width: number;
  thumb_height: number;
  thumb_format: 'unknown' | 'jpeg' | 'bitmap' | 'bitmap16' | 'layer' | 'rollei' | 'h265';
  gps_data?: GpsData;
  color_data?: Record<string, unknown>;
  metadata_common?: MetadataCommon;
  canon?: CanonMetadata;
  nikon?: NikonMetadata;
  fuji?: FujiMetadata;
  sony?: SonyMetadata;
  panasonic?: PanasonicMetadata;
  olympus?: OlympusMetadata;
  pentax?: PentaxMetadata;
  hasselblad?: HasselbladMetadata;
  ricoh?: RicohMetadata;
  kodak?: KodakMetadata;
  p1?: PhaseOneMetadata;
  samsung?: SamsungMetadata;
  [key: string]: unknown;
}

/**
 * Processed RAW image data returned by `imageData()`.
 */
export interface RawImageData {
  width: number;
  height: number;
  colors: number;
  bits: number;
  dataSize: number;
  data: RawPixelData;
}

/**
 * Thumbnail image data returned by `thumbnailData()`.
 */
export interface ThumbnailImageData {
  data: Uint8Array;
  width: number;
  height: number;
  format: 'jpeg' | 'bitmap' | 'unknown';
}

/**
 * Compatibility alias for metadata returned by {@link LibRaw.metadata}.
 */
export type LibRawMetadata = Metadata;

/**
 * Compatibility alias for processed RAW image data.
 */
export type LibRawImageData = RawImageData;

/**
 * Compatibility alias for thumbnail image data.
 */
export type LibRawThumbnailData = ThumbnailImageData;

/**
 * Compatibility alias for settings used when decoding a RAW image.
 */
export type LibRawOptions = LibRawSettings;

/**
 * LibRaw-Wasm decoder.
 *
 * The runtime is asynchronous: construct an instance, open a RAW buffer, then read
 * metadata or decoded image data.
 */
export default class LibRaw {
  /** Create a decoder instance. */
  constructor();

  /**
   * Open and decode a RAW buffer.
   */
  open(bytes: BufferSource, settings?: LibRawSettings): Promise<void>;

  /**
   * Fetch metadata extracted from the opened RAW file.
   *
   * @param fullOutput include vendor-specific metadata blocks (see {@link Metadata.canon}, {@link Metadata.nikon}, etc.) 
   */
  metadata(fullOutput?: boolean): Promise<LibRawMetadata | undefined>;

  /**
   * Fetch processed RAW image data for the opened file.
   */
  imageData(): Promise<LibRawImageData | undefined>;

  /**
   * Fetch the embedded thumbnail preview, when available.
   */
  thumbnailData(): Promise<LibRawThumbnailData | undefined>;
}
