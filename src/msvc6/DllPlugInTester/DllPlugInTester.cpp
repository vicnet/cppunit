#include <cppunit/TestPath.h>
#include <cppunit/TestResult.h>
#include <cppunit/TestResultCollector.h>
#include <cppunit/TestRunner.h>
#include <cppunit/TextTestProgressListener.h>
#include <cppunit/CompilerOutputter.h>
#include <tchar.h>
#include <msvc6/testrunner/TestPlugInInterface.h>
#include <stdexcept>


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#ifndef _UNICODE
#define TCERR std::cerr
#else
#define TCERR std::wcerr
#endif


/*! Converts a ansi string to a TCHAR string.
 */
std::basic_string<TCHAR>
toVariableString( const char *text )
{
#ifdef _UNICODE
  int textLength = ::strlen( text );

  wchar_t *unicodeString = new wchar_t[ textLength + 1 ];
	
  ::MultiByteToWideChar( CP_THREAD_ACP, MB_PRECOMPOSED, 
                         text, textLength,
		                     unicodeString, textLength + 1 );

	std::wstring str( unicodeString );
  delete[] unicodeString;
  return str;
#else
  return text;
#endif
}


/*! Converts a TCHAR string to an ANSI string.
 */
std::string 
toAnsiString( const TCHAR *text )
{
#ifdef _UNICODE
  int bufferLength = ::WideCharToMultiByte( CP_THREAD_ACP, 0, 
                                            text, text.GetLength(),
                                            NULL, 0, NULL, NULL ) +1;
  char *ansiString = new char[bufferLength];
  ::WideCharToMultiByte( CP_THREAD_ACP, 0, 
                         text, text.GetLength(),
                         ansiString, bufferLength, 
                                            NULL,
                                            NULL );

  std::string str( ansiString, bufferLength-1 );
  delete[] ansiString;

  return str;
#else
  return std::string( text );
#endif
}


/*! Runs the specified tests located in the root suite.
 * \param root Root suite that contains all the test of the DLL.
 * \param testPaths Array of string that contains the test paths of all the test to run.
 * \param numberOfPath Number of test paths in \a testPaths. If 0 then \a root suite
 *                     is run.
 * \return \c true if the run succeed, \c false if a test failed or if a test
 *         path was not resolved.
 */
bool 
runDllTest( CppUnit::Test *root,
            TCHAR *testPaths[],
            int numberOfPath )
{
  CppUnit::TestResult controller;
  CppUnit::TestResultCollector result;
  controller.addListener( &result );        
  CppUnit::TextTestProgressListener progress;
  controller.addListener( &progress );      

  CppUnit::TestRunner runner;
  if ( numberOfPath == 0 )
    runner.addTest( root );
  else
  {
    for ( int index =0; index < numberOfPath; ++index )
    {
      const TCHAR *testPath = testPaths[index];
      try
      {
        runner.addTest( root->resolveTestPath( toAnsiString( testPath ) ).getChildTest() );
      }
      catch ( std::invalid_argument & )
      {
        TCERR  <<  _T("Failed to resolve test path: ")  <<  testPath  <<  std::endl;
        return false;
      }
    }
  }

  runner.run( controller );

  std::cerr << std::endl;

  CppUnit::CompilerOutputter outputter( &result, std::cerr );
  outputter.write();                      

  return result.wasSuccessful();
}


/*! Main
 * 
 * Usage: 
 *
 * DllPlugInTester.exe dll-filename [testpath1] [testpath2]...
 *
 * <em>dll-filename</em> must be the name of the DLL. If the DLL use some other DLL, they
 * should be in the path or in the same directory as the DLL. The DLL must export
 * a function named "GetTestPlugInInterface" with the signature
 * GetTestPlugInInterfaceFunction. Both are defined in:
 * \code
 * #include <msvc6/testrunner/TestPlugInInterface.h>
 * \endcode.
 *
 * See examples/msvc6/TestPlugIn for an example of post-build testing.
 *
 * If no test path is specified, they all the test of the suite returned by the DLL
 * are run.
 *
 * You can specify as much test path as you which. Only the test specified by the
 * test paths will be run. Test paths are resolved using Test::resolveTestPath() on
 * the suite returned by the DLL.
 * 
 * If all test succeed and no error happen then the application exit with code 0.
 * If any error occurs (failed to load dll, failed to resolve test paths) or a 
 * test fail, the application exit with code 1.
 */
int 
_tmain( int argc, 
        TCHAR* argv[], 
        TCHAR* envp[] )
{
  const int successReturnCode = 0;
  const int failureReturnCode = 1;

  // check command line
  const TCHAR *applicationName = argv[0];
  if ( argc < 2 )
  {
    TCERR  <<  _T("Usage: ")  <<  std::endl
           <<  applicationName 
           <<  " dll-filename [test-path] [test-path]..."
           <<  std::endl;
    return failureReturnCode;
  }

  // open the dll
  const TCHAR *dllFileName = argv[1];
  HINSTANCE dllHandle = ::LoadLibrary( dllFileName );
  if ( dllHandle == NULL )
  {
    TCERR  <<  "Failed to load dll: "  <<  dllFileName  <<  std::endl;
    return failureReturnCode;
  } 

  // get the plug in function in the dll
  const TCHAR *plugFunctionName = _T("GetTestPlugInInterface");
  GetTestPlugInInterfaceFunction plug = (GetTestPlugInInterfaceFunction)
      ::GetProcAddress( dllHandle, plugFunctionName );
  bool wasSuccessful = false;
  try
  {
    if ( plug != NULL )
    {
      TestPlugInInterface *theInterface = (*plug)();
      wasSuccessful = runDllTest( theInterface->makeTest(), argv+2, argc-2 );
    }
    else
    {
      TCERR  <<  _T("Failed to find exported function named '")
             <<  plugFunctionName  <<  _T("'")
             <<  std::endl;
    }
  }
  catch ( std::exception &e )
  {
    TCERR  <<  _T( "Unexpected exception when running test. You should report this"
                   " and the context which produced the exception:" )  
           << std::endl
           << toVariableString( e.what() )
           << std::endl;
  }

  ::FreeLibrary( dllHandle );

  return wasSuccessful ? successReturnCode : failureReturnCode;
}


