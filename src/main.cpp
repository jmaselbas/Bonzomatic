#include <stdio.h>
#include <string.h>
#include <assert.h>

#ifdef _WIN32
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#endif

#include "ShaderEditor.h"
#include "Renderer.h"
#include "FFT.h"
#include "MIDI.h"
#include "Timer.h"
#include "Misc.h"
#include "UniConversion.h"
#include "jsonxx.h"
#include "Capture.h"
#include "Network.h"
#include "NetworkSettings.h"
#include "Cmdline.h"

unsigned int ParseColor(const std::string& color) {
  if (color.size() < 6 || color.size() > 8) return 0xFFFFFFFF;
  if (color.size() == 6)
  {
    std::string text = "0x" + color;
    unsigned int v = std::stoul(text, 0, 16);
    return (0xFF000000 | ((v & 0xFF0000) >> 16) | (v & 0x00FF00) | ((v & 0x0000FF) << 16));
  }
  else
  {
    std::string text = "0x" + color;
    unsigned int v = std::stoul(text, 0, 16);
    return ((v & 0xFF000000) | ((v & 0x00FF0000) >> 16) | (v & 0x0000FF00) | ((v & 0x000000FF) << 16));
  }
}

void ReplaceTokens( std::string &sDefShader, const char * sTokenBegin, const char * sTokenName, const char * sTokenEnd, std::vector<std::string> &tokens )
{
  if (sDefShader.find(sTokenBegin) != std::string::npos
    && sDefShader.find(sTokenName) != std::string::npos
    && sDefShader.find(sTokenEnd) != std::string::npos
    && sDefShader.find(sTokenBegin) < sDefShader.find(sTokenName)
    && sDefShader.find(sTokenName) < sDefShader.find(sTokenEnd))
  {
    int nTokenStart = (int)(sDefShader.find(sTokenBegin) + strlen(sTokenBegin));
    std::string sTextureToken = sDefShader.substr( nTokenStart, sDefShader.find(sTokenEnd) - nTokenStart );

    std::string sFinalShader;
    sFinalShader = sDefShader.substr( 0, sDefShader.find(sTokenBegin) );

    //for (std::map<std::string, Renderer::Texture*>::iterator it = tokens.begin(); it != tokens.end(); it++)
    for (int i=0; i < tokens.size(); i++)
    {
      std::string s = sTextureToken;
      while (s.find(sTokenName) != std::string::npos)
      {
        s.replace( s.find(sTokenName), strlen(sTokenName), tokens[i], 0, std::string::npos );
      }
      sFinalShader += s;
    }
    sFinalShader += sDefShader.substr( sDefShader.find(sTokenEnd) + strlen(sTokenEnd), std::string::npos );
    sDefShader = sFinalShader;
  }
}

bool CmdHasOption(int argc, const char *argv[], const std::string Option, std::string* Parameter) {
  // start from 2nd param, first is exe name
  for (int i = 1; i < argc; ++i) {
    std::string strarg = argv[i];
    size_t foundEqual = strarg.find('=');
    if (foundEqual == std::string::npos) {
      if (strarg == Option) {
        return true;
      }
    }
    else {
      std::string stroption = strarg.substr(0, foundEqual);
      if (stroption == Option) {
        if (Parameter) {
          (*Parameter) = strarg.substr(foundEqual + 1, std::string::npos);
        }
        return true;
      }
    }
  }
  return false;
}

void Network_Break_URL(std::string ServerURL, std::string& ServerName, std::string& RoomName, std::string& NickName) {
  
    std::size_t LastPart = ServerURL.rfind('/');
    if(LastPart == std::string::npos || LastPart<=7) {
      ServerName = ServerURL;
    } else {
      std::size_t SecondPart = ServerURL.rfind('/', LastPart-1);
      if(SecondPart == std::string::npos || SecondPart<=7) {
        ServerName = ServerURL.substr(0,LastPart);
        RoomName = ServerURL.substr(LastPart+1);
      } else {
        ServerName = ServerURL.substr(0,SecondPart);
        RoomName = ServerURL.substr(SecondPart+1, LastPart-SecondPart-1);
        NickName = ServerURL.substr(LastPart+1);
      }
    }
}

int main(int argc, const char *argv[])
{
  Misc::PlatformStartup();

  std::string configFile = "config.json";
  
  if (CmdHasOption(argc, argv, "configfile", &configFile)) {
    printf("Loading config file '%s'...\n", configFile);
  } else {
    char configPath[256] = { 0 };
    if (getcwd(configPath, 255))
    {
      printf("Looking for config.json in '%s'...\n", configPath);
    }
  }
  
  jsonxx::Object options;
  FILE * fConf = fopen( configFile.c_str(), "rb" );
  if (fConf)
  {
    printf("Config file found, parsing...\n");

    char szConfig[65535];
    memset( szConfig, 0, 65535 );
    fread( szConfig, 1, 65535, fConf );
    fclose(fConf);

    options.parse( szConfig );
  }

  bool SkipConfigDialog = CmdHasOption(argc, argv, "skipdialog");
  
  RENDERER_SETTINGS settings;
  settings.bVsync = false;

  NETWORK_SETTINGS netSettings;
  if (options.has<jsonxx::Object>("network"))
		{
			jsonxx::Object netjson = options.get<jsonxx::Object>("network");
			if (netjson.has<jsonxx::Boolean>("enabled"))
				netSettings.EnableNetwork = netjson.get<jsonxx::Boolean>("enabled");
			if (netjson.has<jsonxx::String>("serverURL"))
				netSettings.ServerURL = netjson.get<jsonxx::String>("serverURL");
			if (netjson.has<jsonxx::String>("networkMode"))
				netSettings.NetworkModeString = netjson.get<jsonxx::String>("networkMode");
		}
//#ifdef _DEBUG
#if 0
  settings.nWidth = 1280;
  settings.nHeight = 720;
  settings.windowMode = RENDERER_WINDOWMODE_WINDOWED;
#else
  settings.nWidth = 1920;
  settings.nHeight = 1080;
  settings.windowMode = RENDERER_WINDOWMODE_FULLSCREEN;
  if (options.has<jsonxx::Object>("window"))
  {
    if (options.get<jsonxx::Object>("window").has<jsonxx::Number>("width"))
      settings.nWidth = options.get<jsonxx::Object>("window").get<jsonxx::Number>("width");
    if (options.get<jsonxx::Object>("window").has<jsonxx::Number>("height"))
      settings.nHeight = options.get<jsonxx::Object>("window").get<jsonxx::Number>("height");
    if (options.get<jsonxx::Object>("window").has<jsonxx::Boolean>("fullscreen"))
      settings.windowMode = options.get<jsonxx::Object>("window").get<jsonxx::Boolean>("fullscreen") ? RENDERER_WINDOWMODE_FULLSCREEN : RENDERER_WINDOWMODE_WINDOWED;
    if (options.get<jsonxx::Object>("window").has<jsonxx::Boolean>("borderlessfullscreen") && options.get<jsonxx::Object>("window").get<jsonxx::Boolean>("borderlessfullscreen"))
      settings.windowMode = RENDERER_WINDOWMODE_BORDERLESS;
  }
  if(!SkipConfigDialog)
  {
    printf("Waiting Config dialog\n");
    if (!Renderer::OpenSetupDialog( &settings, &netSettings ))
      return -1;
  }
#endif
  
  Network::CommandLine(argc, argv, &netSettings);
  Network::LoadSettings(options, &netSettings);
  Network::OpenConnection();

  if (!Renderer::Open( &settings ))
  {
    printf("Renderer::Open failed\n");
    return -1;
  }

  if (!FFT::Open())
  {
    printf("FFT::Open() failed, continuing anyway...\n");
    //return -1;
  }

  if (!MIDI::Open())
  {
    printf("MIDI::Open() failed, continuing anyway...\n");
    //return -1;
  }

  std::map<std::string,Renderer::Texture*> textures;
  std::map<int,std::string> midiRoutes;

  const char * szDefaultFontPath = Misc::GetDefaultFontPath();

  SHADEREDITOR_OPTIONS editorOptions;
  editorOptions.nFontSize = 16;
  if ( !szDefaultFontPath )
  {
    printf( "Misc::GetDefaultFontPath couldn't find ANY default fonts!\n" );
  }
  else
  {
    editorOptions.sFontPath = szDefaultFontPath;
  }
  editorOptions.nOpacity = 0xC0;
  editorOptions.bUseSpacesForTabs = true;
  editorOptions.nTabSize = 2;
  editorOptions.bVisibleWhitespace = false;
  editorOptions.eAutoIndent = aitSmart;

  int nDebugOutputHeight = 200;
  int nTexPreviewWidth = 64;
  float fFFTSmoothingFactor = 0.9f; // higher value, smoother FFT
  float fFFTSlightSmoothingFactor = 0.6f; // higher value, smoother FFT
  float fScrollXFactor = 1.0f;
  float fScrollYFactor = 1.0f;

  std::string sPostExitCmd;

  if (!options.empty())
  {
    if (options.has<jsonxx::Object>("rendering"))
    {
      if (options.get<jsonxx::Object>("rendering").has<jsonxx::Number>("fftSmoothFactor"))
        fFFTSmoothingFactor = options.get<jsonxx::Object>("rendering").get<jsonxx::Number>("fftSmoothFactor");
      if (options.get<jsonxx::Object>("rendering").has<jsonxx::Number>("fftAmplification"))
        FFT::fAmplification = options.get<jsonxx::Object>("rendering").get<jsonxx::Number>("fftAmplification");
    }

    if (options.has<jsonxx::Object>("textures"))
    {
      printf("Loading textures...\n");
      std::map<std::string, jsonxx::Value*> tex = options.get<jsonxx::Object>("textures").kv_map();
      for (std::map<std::string, jsonxx::Value*>::iterator it = tex.begin(); it != tex.end(); it++)
      {
        const char * fn = it->second->string_value_->c_str();
        printf("* %s...\n",fn);
        Renderer::Texture * tex = Renderer::CreateRGBA8TextureFromFile( fn );
        if (!tex)
        {
          printf("Renderer::CreateRGBA8TextureFromFile(%s) failed\n",fn);
          return -1;
        }
        textures[it->first] = tex;
      }
    }
    if (options.has<jsonxx::Object>("font"))
    {
      if (options.get<jsonxx::Object>("font").has<jsonxx::Number>("size"))
        editorOptions.nFontSize = options.get<jsonxx::Object>("font").get<jsonxx::Number>("size");
      if (options.get<jsonxx::Object>("font").has<jsonxx::String>("file"))
      {
        std::string fontpath = options.get<jsonxx::Object>("font").get<jsonxx::String>("file");
        if (!Misc::FileExists(fontpath.c_str()))
        {
          printf("Font path (%s) is invalid!\n", fontpath.c_str());
          return -1;
        }
        editorOptions.sFontPath = fontpath;
      }
    }
    if (options.has<jsonxx::Object>("gui"))
    {
      if (options.get<jsonxx::Object>("gui").has<jsonxx::Number>("outputHeight"))
        nDebugOutputHeight = options.get<jsonxx::Object>("gui").get<jsonxx::Number>("outputHeight");
      if (options.get<jsonxx::Object>("gui").has<jsonxx::Number>("texturePreviewWidth"))
        nTexPreviewWidth = options.get<jsonxx::Object>("gui").get<jsonxx::Number>("texturePreviewWidth");
      if (options.get<jsonxx::Object>("gui").has<jsonxx::Number>("opacity"))
        editorOptions.nOpacity = options.get<jsonxx::Object>("gui").get<jsonxx::Number>("opacity");
      if (options.get<jsonxx::Object>("gui").has<jsonxx::Boolean>("spacesForTabs"))
        editorOptions.bUseSpacesForTabs = options.get<jsonxx::Object>("gui").get<jsonxx::Boolean>("spacesForTabs");
      if (options.get<jsonxx::Object>("gui").has<jsonxx::Number>("tabSize"))
        editorOptions.nTabSize = options.get<jsonxx::Object>("gui").get<jsonxx::Number>("tabSize");
      if (options.get<jsonxx::Object>("gui").has<jsonxx::Boolean>("visibleWhitespace"))
        editorOptions.bVisibleWhitespace = options.get<jsonxx::Object>("gui").get<jsonxx::Boolean>("visibleWhitespace");
      if (options.get<jsonxx::Object>("gui").has<jsonxx::String>("autoIndent"))
      {
        std::string autoIndent = options.get<jsonxx::Object>("gui").get<jsonxx::String>("autoIndent");
        if (autoIndent == "smart") {
          editorOptions.eAutoIndent = aitSmart;
        } else if (autoIndent == "preserve") {
          editorOptions.eAutoIndent = aitPreserve;
        } else {
          editorOptions.eAutoIndent = aitNone;
        }
      }
      if (options.get<jsonxx::Object>("gui").has<jsonxx::Number>("scrollXFactor"))
        fScrollXFactor = options.get<jsonxx::Object>("gui").get<jsonxx::Number>("scrollXFactor");
      if (options.get<jsonxx::Object>("gui").has<jsonxx::Number>("scrollYFactor"))
        fScrollYFactor = options.get<jsonxx::Object>("gui").get<jsonxx::Number>("scrollYFactor");
    }
    if (options.has<jsonxx::Object>("theme"))
    {
      const auto& theme = options.get<jsonxx::Object>("theme");
      if (theme.has<jsonxx::String>("text"))
        editorOptions.theme.text = ParseColor(theme.get<jsonxx::String>("text"));
      if (theme.has<jsonxx::String>("comment"))
        editorOptions.theme.comment = ParseColor(theme.get<jsonxx::String>("comment"));
      if (theme.has<jsonxx::String>("number"))
        editorOptions.theme.number = ParseColor(theme.get<jsonxx::String>("number"));
      if (theme.has<jsonxx::String>("op"))
        editorOptions.theme.op = ParseColor(theme.get<jsonxx::String>("op"));
      if (theme.has<jsonxx::String>("keyword"))
        editorOptions.theme.keyword = ParseColor(theme.get<jsonxx::String>("keyword"));
      if (theme.has<jsonxx::String>("type"))
        editorOptions.theme.type = ParseColor(theme.get<jsonxx::String>("type"));
      if (theme.has<jsonxx::String>("builtin"))
        editorOptions.theme.builtin = ParseColor(theme.get<jsonxx::String>("builtin"));
      if (theme.has<jsonxx::String>("preprocessor"))
        editorOptions.theme.preprocessor = ParseColor(theme.get<jsonxx::String>("preprocessor"));
      if (theme.has<jsonxx::String>("selection"))
        editorOptions.theme.selection = ParseColor(theme.get<jsonxx::String>("selection"));
      if (theme.has<jsonxx::String>("charBackground")) {
        editorOptions.theme.bUseCharBackground = true;
        editorOptions.theme.charBackground = ParseColor(theme.get<jsonxx::String>("charBackground"));
      }
    }
    if (options.has<jsonxx::Object>("midi"))
    {
      std::map<std::string, jsonxx::Value*> tex = options.get<jsonxx::Object>("midi").kv_map();
      for (std::map<std::string, jsonxx::Value*>::iterator it = tex.begin(); it != tex.end(); it++)
      {
        midiRoutes[it->second->number_value_] = it->first;
      }
    }
    if (options.has<jsonxx::String>("postExitCmd"))
    {
      sPostExitCmd = options.get<jsonxx::String>("postExitCmd");
    }
    Capture::LoadSettings( options );
  }
  if (!editorOptions.sFontPath.size())
  {
    printf("Couldn't find any of the default fonts. Please specify one in config.json\n");
    return -1;
  }
  if (!Capture::Open(settings))
  {
    printf("Initializing capture system failed!\n");
    return 0;
  }
  
  Renderer::Texture * texFFT = Renderer::Create1DR32Texture( FFT_SIZE );
  Renderer::Texture * texFFTSmoothed = Renderer::Create1DR32Texture( FFT_SIZE );
  Renderer::Texture * texFFTIntegrated = Renderer::Create1DR32Texture( FFT_SIZE );

  std::string shaderFileName = Renderer::defaultShaderFilename;
  if (Network::IsNetworkEnabled())
  {
    std::string ServerName;
    std::string RoomName;
    std::string NickName;
    Network_Break_URL(netSettings.ServerURL, ServerName, RoomName, NickName);
    if(RoomName.length()>0) {
      if(NickName.length()>0) {
        shaderFileName = RoomName + "_" + NickName + Renderer::defaultShaderExtention;
      } else {
        shaderFileName = RoomName + Renderer::defaultShaderExtention;
      }
    }
  }
  if (CmdHasOption(argc, argv, "shader", &shaderFileName)) {
    printf("Loading Shader: %s \n", shaderFileName.c_str());
  }

  bool shaderInitSuccessful = false;
  char szShader[65535];
  char szError[4096];
  FILE * f = fopen(shaderFileName.c_str(),"rb");
  if (f)
  {
    printf("Loading last shader...\n");

    memset( szShader, 0, 65535 );
    fread( szShader, 1, 65535, f );
    fclose(f);
    if (Renderer::ReloadShader( szShader, (int)strlen(szShader), szError, 4096 ))
    {
      printf("Last shader works fine.\n");
      shaderInitSuccessful = true;
    }
    else {
      printf("Shader error:\n%s\n", szError);
    }
  }
  if (!shaderInitSuccessful)
  {
    printf("No valid last shader found, falling back to default...\n");

    std::string sDefShader = Renderer::defaultShader;

    std::vector<std::string> tokens;
    for (std::map<std::string, Renderer::Texture*>::iterator it = textures.begin(); it != textures.end(); it++)
      tokens.push_back(it->first);
    ReplaceTokens(sDefShader, "{%textures:begin%}", "{%textures:name%}", "{%textures:end%}", tokens);

    tokens.clear();
    for (std::map<int,std::string>::iterator it = midiRoutes.begin(); it != midiRoutes.end(); it++)
      tokens.push_back(it->second);
    ReplaceTokens(sDefShader, "{%midi:begin%}", "{%midi:name%}", "{%midi:end%}", tokens);

    strncpy( szShader, sDefShader.c_str(), 65535 );
    if (!Renderer::ReloadShader( szShader, (int)strlen(szShader), szError, 4096 ))
    {
      printf("Default shader compile failed:\n");
      puts(szError);
      assert(0);
    }
  }

  Misc::InitKeymaps();

#ifdef SCI_LEXER
  Scintilla_LinkLexers();
#endif
  Scintilla::Surface * surface = Scintilla::Surface::Allocate( SC_TECHNOLOGY_DEFAULT );
  surface->Init( NULL );

  int nMargin = 20;

  bool bTexPreviewVisible = true;

  editorOptions.rect = Scintilla::PRectangle( nMargin, nMargin, settings.nWidth - nMargin - nTexPreviewWidth - nMargin, settings.nHeight - nMargin * 2 - nDebugOutputHeight );
  ShaderEditor mShaderEditor( surface );
  mShaderEditor.Initialise( editorOptions );
  mShaderEditor.SetText( szShader );

  editorOptions.rect = Scintilla::PRectangle( nMargin, settings.nHeight - nMargin - nDebugOutputHeight, settings.nWidth - nMargin - nTexPreviewWidth - nMargin, settings.nHeight - nMargin );
  ShaderEditor mDebugOutput( surface );
  mDebugOutput.Initialise( editorOptions );
  mDebugOutput.SetText( "" );
  mDebugOutput.SetReadOnly(true);

  static float fftData[FFT_SIZE];
  memset(fftData, 0, sizeof(float) * FFT_SIZE);
  static float fftDataSmoothed[FFT_SIZE];
  memset(fftDataSmoothed, 0, sizeof(float) * FFT_SIZE);


  static float fftDataSlightlySmoothed[FFT_SIZE];
  memset(fftDataSlightlySmoothed, 0, sizeof(float) * FFT_SIZE);
  static float fftDataIntegrated[FFT_SIZE];
  memset(fftDataIntegrated, 0, sizeof(float) * FFT_SIZE);

  bool bShowGui = true;
  Timer::Start();
  float fNextTick = 0.1f;
  float oldtime = Timer::GetTime() / 1000.0;  
  while (!Renderer::WantsToQuit())
  {
    
    bool newShader = false;
    bool needEditorUpdate = false;
    float time = Timer::GetTime() / 1000.0; // seconds
    Renderer::StartFrame();

    // Networking
    Network::Tick();
    
    if (Network::IsShaderNeedUpdate(time))
    {
      mShaderEditor.GetText(szShader, 65535);
	  Network::ShaderMessage NewMessage;
	  NewMessage.Code = std::string(szShader);
	  NewMessage.NeedRecompile = false;
	  NewMessage.CaretPosition = mShaderEditor.WndProc(SCI_GETCURRENTPOS, 0, 0);
	  NewMessage.AnchorPosition = mShaderEditor.WndProc(SCI_GETANCHOR, 0, 0);
      Network::SendShader(time, NewMessage);
	}
    
    for(int i=0; i<Renderer::mouseEventBufferCount; i++)
    {
      if (bShowGui)
      {
		bool IsGrabber = Network::IsConnected() && Network::IsGrabber();
        switch (Renderer::mouseEventBuffer[i].eventType)
        {
          case Renderer::MOUSEEVENTTYPE_MOVE:
            mShaderEditor.ButtonMovePublic( Scintilla::Point( Renderer::mouseEventBuffer[i].x, Renderer::mouseEventBuffer[i].y ) );
            break;
          case Renderer::MOUSEEVENTTYPE_DOWN:
            if(!IsGrabber) mShaderEditor.ButtonDown( Scintilla::Point( Renderer::mouseEventBuffer[i].x, Renderer::mouseEventBuffer[i].y ), time * 1000, false, false, false );
            break;
          case Renderer::MOUSEEVENTTYPE_UP:
            if(!IsGrabber) mShaderEditor.ButtonUp( Scintilla::Point( Renderer::mouseEventBuffer[i].x, Renderer::mouseEventBuffer[i].y ), time * 1000, false );
            break;
          case Renderer::MOUSEEVENTTYPE_SCROLL:
            mShaderEditor.WndProc( SCI_LINESCROLL, (int)(-Renderer::mouseEventBuffer[i].x * fScrollXFactor), (int)(-Renderer::mouseEventBuffer[i].y * fScrollYFactor));
            break;
        }
      }
    }
    Renderer::mouseEventBufferCount = 0;

    if (Network::HasNewShader()) {
		Network::ShaderMessage NewMessage;
		if(Network::GetNewShader(NewMessage)) {
			int PreviousTopLine = mShaderEditor.WndProc(SCI_GETFIRSTVISIBLELINE, 0, 0);
			mShaderEditor.SetText(NewMessage.Code.c_str());
			mShaderEditor.WndProc(SCI_SETCURRENTPOS, NewMessage.CaretPosition, 0);
			mShaderEditor.WndProc(SCI_SETANCHOR, NewMessage.AnchorPosition, 0);
			mShaderEditor.WndProc(SCI_SETFIRSTVISIBLELINE, PreviousTopLine, 0);
			if(NewMessage.NeedRecompile) {
				mShaderEditor.GetText(szShader, 65535);
				if (Renderer::ReloadShader(szShader, (int)strlen(szShader), szError, 4096))
				{
					// Shader compilation successful; we set a flag to save if the frame render was successful
					// (If there is a driver crash, don't save.)
					newShader = true;
				}
				else
				{
					mDebugOutput.SetText(szError);
				}
			}
		}
    }

    for(int i=0; i<Renderer::keyEventBufferCount; i++)
    {
      if (Renderer::keyEventBuffer[i].scanCode == 283) // F2
      {
         bTexPreviewVisible = !bTexPreviewVisible;
         needEditorUpdate = true;
      }
      else if (Renderer::keyEventBuffer[i].scanCode == 286 || (Renderer::keyEventBuffer[i].ctrl && Renderer::keyEventBuffer[i].scanCode == 'r')) // F5
      {
        mShaderEditor.GetText(szShader,65535);
		Network::ShaderMessage NewMessage;
		NewMessage.Code = std::string(szShader);
		NewMessage.NeedRecompile = true;
		NewMessage.CaretPosition = mShaderEditor.WndProc(SCI_GETCURRENTPOS, 0, 0);
		NewMessage.AnchorPosition = mShaderEditor.WndProc(SCI_GETANCHOR, 0, 0);
		Network::SendShader(time, NewMessage);

        if (Renderer::ReloadShader( szShader, (int)strlen(szShader), szError, 4096 ))
        {
          // Shader compilation successful; we set a flag to save if the frame render was successful
          // (If there is a driver crash, don't save.)
          newShader = true;
        }
        else
        {
          mDebugOutput.SetText( szError );
        }
      }
      else if (Renderer::keyEventBuffer[i].scanCode == 292 || (Renderer::keyEventBuffer[i].ctrl && Renderer::keyEventBuffer[i].scanCode == 'f')) // F11 or Ctrl/Cmd-f  
      {
        bShowGui = !bShowGui;
      }
      else if (bShowGui)
      {
        if (!Network::IsConnected() || !Network::IsGrabber())
        {
          bool consumed = false;
          if (Renderer::keyEventBuffer[i].scanCode)
          {
            mShaderEditor.KeyDown(
              iswalpha(Renderer::keyEventBuffer[i].scanCode) ? towupper(Renderer::keyEventBuffer[i].scanCode) : Renderer::keyEventBuffer[i].scanCode,
              Renderer::keyEventBuffer[i].shift,
              Renderer::keyEventBuffer[i].ctrl,
              Renderer::keyEventBuffer[i].alt,
              &consumed);
          }
          if (!consumed && Renderer::keyEventBuffer[i].character)
          {
            char    utf8[5] = { 0,0,0,0,0 };
            wchar_t utf16[2] = { Renderer::keyEventBuffer[i].character, 0 };
            Scintilla::UTF8FromUTF16(utf16, 1, utf8, 4 * sizeof(char));
            mShaderEditor.AddCharUTF(utf8, (unsigned int)strlen(utf8));
          }
        }
      }
    }
    Renderer::keyEventBufferCount = 0;

    Renderer::SetShaderConstant( "fGlobalTime", time );
    Renderer::SetShaderConstant( "v2Resolution", Renderer::nWidth, Renderer::nHeight );

    for (std::map<int,std::string>::iterator it = midiRoutes.begin(); it != midiRoutes.end(); it++)
    {
      Renderer::SetShaderConstant( it->second.c_str(), MIDI::GetCCValue( it->first ) );
    }


    if (FFT::GetFFT(fftData))
    {
      Renderer::UpdateR32Texture( texFFT, fftData );

      const static float maxIntegralValue = 1024.0f;
      for ( int i = 0; i < FFT_SIZE; i++ )
      {
        fftDataSmoothed[i] = fftDataSmoothed[i] * fFFTSmoothingFactor + (1 - fFFTSmoothingFactor) * fftData[i];

        fftDataSlightlySmoothed[i] = fftDataSlightlySmoothed[i] * fFFTSlightSmoothingFactor + (1 - fFFTSlightSmoothingFactor) * fftData[i];
        fftDataIntegrated[i] = fftDataIntegrated[i] + fftDataSlightlySmoothed[i];
        if (fftDataIntegrated[i] > maxIntegralValue) {
          fftDataIntegrated[i] -= maxIntegralValue;
        }
      }

      Renderer::UpdateR32Texture( texFFTSmoothed, fftDataSmoothed );
      Renderer::UpdateR32Texture( texFFTIntegrated, fftDataIntegrated );
    }

    Renderer::SetShaderTexture( "texFFT", texFFT );
    Renderer::SetShaderTexture( "texFFTSmoothed", texFFTSmoothed );
    Renderer::SetShaderTexture( "texFFTIntegrated", texFFTIntegrated );

    for (std::map<std::string, Renderer::Texture*>::iterator it = textures.begin(); it != textures.end(); it++)
    {
      Renderer::SetShaderTexture( it->first.c_str(), it->second );
    }

    Renderer::RenderFullscreenQuad();

    if (needEditorUpdate || Renderer::nSizeChanged)
    {
      int TexPreviewOffset = bTexPreviewVisible ? nTexPreviewWidth + nMargin : 0;
      mShaderEditor.SetPosition(Scintilla::PRectangle(nMargin, nMargin, Renderer::nWidth - nMargin - TexPreviewOffset, Renderer::nHeight - nMargin * 2 - nDebugOutputHeight));
      mDebugOutput.SetPosition(Scintilla::PRectangle(nMargin, Renderer::nHeight - nMargin - nDebugOutputHeight, Renderer::nWidth - nMargin - TexPreviewOffset, Renderer::nHeight - nMargin));
    }
    
    Renderer::StartTextRendering();

    if (bShowGui)
    {
      if (time > fNextTick)
      {
        mShaderEditor.Tick();
        mDebugOutput.Tick();
        fNextTick = time + 0.1;
      }

      mShaderEditor.Paint();
      mDebugOutput.Paint();

      Renderer::SetTextRenderingViewport( Scintilla::PRectangle(0,0,Renderer::nWidth,Renderer::nHeight) );

      if (bTexPreviewVisible)
      {
        int y1 = nMargin;
        int x1 = Renderer::nWidth - nMargin - nTexPreviewWidth;
        int x2 = Renderer::nWidth - nMargin;
        for (std::map<std::string, Renderer::Texture*>::iterator it = textures.begin(); it != textures.end(); it++)
        {
          int y2 = y1 + nTexPreviewWidth * (it->second->height / (float)it->second->width);
          Renderer::BindTexture( it->second );
          Renderer::RenderQuad(
            Renderer::Vertex( x1, y1, 0xccFFFFFF, 0.0, 0.0 ),
            Renderer::Vertex( x2, y1, 0xccFFFFFF, 1.0, 0.0 ),
            Renderer::Vertex( x2, y2, 0xccFFFFFF, 1.0, 1.0 ),
            Renderer::Vertex( x1, y2, 0xccFFFFFF, 0.0, 1.0 )
          );
          surface->DrawTextNoClip( Scintilla::PRectangle(x1,y1,x2,y2), *mShaderEditor.GetTextFont(), y2 - 5.0, it->first.c_str(), (int)it->first.length(), 0xffFFFFFF, 0x00000000);
          y1 = y2 + nMargin;
        }
      }

      /*
      char szLayout[255];
      Misc::GetKeymapName(szLayout);
      std::string sHelp = "F2 - toggle texture preview   F5 or Ctrl-R - recompile shader   F11 - hide GUI   Current keymap: ";
      sHelp += szLayout;
      */

      char frame_ms[10];
      snprintf(frame_ms, sizeof(frame_ms), "%.2f", (time - oldtime) * 1000.0f);
      char frame_fps[10];
      snprintf(frame_fps, sizeof(frame_fps), "%.2f", 1.0f/(time - oldtime));
      std::string sHelp = "Bonzomatic GLSL ( ";
      sHelp += frame_ms;
      sHelp += "ms ";
      sHelp += frame_fps;
      sHelp += "fps )";
      surface->DrawTextNoClip(Scintilla::PRectangle(20, Renderer::nHeight - 20, 100, Renderer::nHeight), *mShaderEditor.GetTextFont(), Renderer::nHeight - 5.0, sHelp.c_str(), (int)sHelp.length(), 0x80FFFFFF, 0x00000000);
    }


    Renderer::EndTextRendering();

    if (Renderer::nSizeChanged)
    {
      Capture::CaptureResize(Renderer::nWidth, Renderer::nHeight);
    }
    Renderer::nSizeChanged = false;

    Renderer::EndFrame();

    Capture::CaptureFrame();

    if (newShader)
    {
      // Frame render successful, save shader
      FILE * f = fopen(shaderFileName.c_str(),"wb");
      if (f)
      {
        fwrite( szShader, strlen(szShader), 1, f );
        fclose(f);
        mDebugOutput.SetText( "" );
      }
      else
      {
        mDebugOutput.SetText( "Unable to save shader! Your work will be lost when you quit!" );
      }
    }
    oldtime = time;
  }


  delete surface;

  MIDI::Close();
  FFT::Close();

  Network::Release();

  Renderer::ReleaseTexture( texFFT );
  Renderer::ReleaseTexture( texFFTSmoothed );
  for (std::map<std::string, Renderer::Texture*>::iterator it = textures.begin(); it != textures.end(); it++)
  {
    Renderer::ReleaseTexture( it->second );
  }

  Renderer::Close();

  if ( !sPostExitCmd.empty() )
  {
    Misc::ExecuteCommand( sPostExitCmd.c_str(), shaderFileName.c_str() );
  }

  Misc::PlatformShutdown();

  return 0;
}
