/*****************************************************************************
 * RRDLIB .NET Binding
 *****************************************************************************
 * Created 2010/06/29 by Chris Larsen
 * 
 * This .NET interface allows the use of Tobias Oetiker's awesome RRDtool 
 * functions in .NET projects using the PInvoke method to load the rrdlib.dll
 * To use, please make sure that you place the rrdlib.dll in the same 
 * directory as this dll, or change the "const string dll" to point to the
 * proper location. For documentation, please see the RRDtool website at:
 * http://oss.oetiker.ch/rrdtool/
 * For usage examples, please see the rrd_binding_test project.
 ****************************************************************************/
using System;
using System.Linq;
using System.Collections.Generic;
using System.Runtime.InteropServices;

#if X64
#error 64-bit platform not yet supported.
#endif

/// <summary>
/// Contains data structures and methods for working with round robin databases.
/// </summary>
namespace dnrrdlib
{
    /// <summary>
    /// Information about a particular RRD parameter. The key is the name of the parameter,
    /// type determines what kind of value we have, value is the value, and next is a 
    /// pointer to another info object.
    /// </summary>
    [StructLayout(LayoutKind.Explicit, Pack = 1)]
    public struct rrd_info_t
    {
        [FieldOffset(0), MarshalAs(UnmanagedType.LPStr)]
        public string key;
        [FieldOffset(4)]    // for 64 bit, set this to 8 and increment everyone else by 4
        public rrd_info_type_t type;
        [FieldOffset(8)]
        public rrd_infoval_t value;
        [FieldOffset(16)]
        public IntPtr next;
    }

    /// <summary>
    /// This is a chunk of data returned from an RRD object
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct rrd_blob_t
    {
        public UInt32 size;     /* size of the blob */
        public IntPtr ptr;    /* pointer */
    };

    /// <summary>
    /// This contains the actual data values for an rrd_info_t structure. 
    /// NOTE: Only one of these will be valid per instance. Use the containing info_t's
    /// type field to determine which of these to read. 
    /// NOTE: If the type is RD_I_STR, you have to marshal the string value yourself
    /// </summary>
    [StructLayout(LayoutKind.Explicit)]
    public struct rrd_infoval_t
    {
        [FieldOffset(0)]
        public UInt32 u_cnt;
        [FieldOffset(0)]
        public double u_val;
        [FieldOffset(0)]
        public IntPtr u_str;
        [FieldOffset(0)]
        public Int32 u_int;
        [FieldOffset(0)]
        public rrd_blob_t u_blo;
    };

    /// <summary>
    /// Different rrd_info_t value types
    /// </summary>
    public enum rrd_info_type_t
    {
        RD_I_VAL = 0,
        RD_I_CNT,
        RD_I_STR,
        RD_I_INT,
        RD_I_BLO
    };

    /// <summary>
    /// Direct bindings to the RRD Library for .NET applications. Uses the PInvoke method
    /// of accessing the rrdlib.dll file.
    /// </summary>
    public class rrd
    {
        // Set this path to the location of your "rrdlib.dll" file
        const string dll = @"librrd-4.dll";

        // IMPORTS - Main methods
        [DllImport(dll)] static extern Int32 rrd_create(Int32 argc, string[] argv);
        [DllImport(dll)] static extern Int32 rrd_create_r([MarshalAs(UnmanagedType.LPStr)] string filename,
            UInt32 pdp_step, Int64 last_up, Int32 argc, [MarshalAs(UnmanagedType.LPArray)] string[] argv);
        [DllImport(dll)] static extern IntPtr rrd_info_r(string filename);
        [DllImport(dll)] static extern void rrd_info_print(IntPtr data);
        [DllImport(dll)] static extern void rrd_info_free(IntPtr data);

        [DllImport(dll)] static extern Int32 rrd_update(Int32 argc, string[] argv);
        [DllImport(dll)] static extern IntPtr rrd_update_v(Int32 argc, string[] argv);
        [DllImport(dll)] static extern Int32 rrd_update_r(string filename, string template, Int32 argc,
            string[] argv);
        /* Do not use this until someone adds the FILE structure */
        [DllImport(dll)] static extern Int32 rrd_graph(Int32 argc, string[] argv, ref string[] prdata,
            ref Int32 xsize, ref Int32 ysize, /* TODO - FILE, */ ref double ymin, ref double ymax);
        [DllImport(dll)] static extern IntPtr rrd_graph_v(Int32 argc, string[] argv);
        [DllImport(dll)] static extern Int32 rrd_fetch(Int32 argc, string[] argv, ref Int64 start,
            ref Int64 end, ref UInt32 step, [Out] out UInt32 ds_cnt, [Out] out IntPtr ds_namv, [Out] out IntPtr data);
        [DllImport(dll)] static extern Int32 rrd_first(Int32 argc, string[] argv);
        [DllImport(dll)] static extern Int32 rrd_first_r(string filename, Int32 rraindex);
        [DllImport(dll)] static extern Int32 rrd_last(Int32 argc, string[] argv);
        [DllImport(dll)] static extern Int32 rrd_last_r(string filename, Int32 rraindex);
        [DllImport(dll)] static extern Int32 rrd_lastupdate(Int32 argc, string[] argv);
        [DllImport(dll)] static extern Int32 rrd_lastupdate_r(string filename, ref Int32 ret_last_update,
            ref UInt32 ret_ds_count, [Out] out IntPtr ret_ds_names, [Out] out IntPtr ret_last_ds);
        [DllImport(dll)] static extern Int32 rrd_dump(Int32 argc, string[] argv);
        [DllImport(dll)] static extern Int32 rrd_dump_r(string filename, string outname);
        [DllImport(dll)] static extern Int32 rrd_xport(Int32 argc, string[] argv, Int32 unused,
            ref Int64 start, ref Int64 end, ref UInt32 step, ref UInt32 col_cnt,
            [Out] out IntPtr leggend_v, [Out] out  IntPtr data);
        [DllImport(dll)] static extern Int32 rrd_restore(Int32 argc, string[] argv);
        [DllImport(dll)] static extern Int32 rrd_resize(Int32 argc, string[] argv);
        [DllImport(dll)] static extern Int32 rrd_tune(Int32 argc, string[] argv);

        // IMPORTS - Utility methods
        [DllImport(dll)] static extern string rrd_strversion();
        [DllImport(dll)] static extern Int32 rrd_random();
        [DllImport(dll)] static extern IntPtr rrd_get_error();
        [DllImport(dll)] internal static extern void rrd_clear_error();

        // MAIN FUNCTIONS

        public static DateTime UnixTimestampToDateTime(Int32 unixTimeStamp)
        {
            return new DateTime(1970, 1, 1, 0, 0, 0, 0).AddSeconds(unixTimeStamp).ToLocalTime();
        }

        public static Int32 DateTimeToUnixTimestamp(DateTime input)
        {
            return (Int32)input.Subtract(new DateTime(1970, 1, 1, 0, 0, 0, 0)).TotalSeconds;
        }

        /// <summary>
        /// The create function of RRDtool lets you set up new Round Robin Database (RRD) files. 
        /// The file is created at its final, full size and filled with *UNKNOWN* data.
        /// </summary>
        /// <param name="argv">String array of command line arguments</param>
        /// <returns>0 if successful, -1 if an error occurred</returns>
        public static int Create(string[] argv)
        {
            return rrd_create(argv.Length, argv);
        }

        /// <summary>
        /// The create function of RRDtool lets you set up new Round Robin Database (RRD) files. 
        /// The file is created at its final, full size and filled with *UNKNOWN* data.
        /// </summary>
        /// <param name="filename">A full path to the location where you want the rrd to reside</param>
        /// <param name="pdp_step">Specifies the base interval in seconds with which data will be fed into the RRD</param>
        /// <param name="last_up">Timestamp of the last update</param>
        /// <param name="argv">String array of command line arguments</param>
        public static void Create(string filename, UInt32 pdp_step, Int32 last_up, string[] argv)
        {
            if (rrd_create_r(filename, pdp_step, last_up, argv.Length, argv) < 0)
            {
                throw new RrdException();
            }
        }

        private static Dictionary<string, object> ConvertInfoToDict(IntPtr ptr)
        {
            var dict = new Dictionary<string, object>();

            rrd_info_t? info = (rrd_info_t)Marshal.PtrToStructure(ptr, typeof(rrd_info_t));

            while (info.HasValue)
            {
                switch (info.Value.type)
                {
                    case rrd_info_type_t.RD_I_STR:
                        dict.Add(info.Value.key, System.Runtime.InteropServices.Marshal.PtrToStringAnsi(info.Value.value.u_str));
                        break;
                    case rrd_info_type_t.RD_I_INT:
                        dict.Add(info.Value.key, info.Value.value.u_int);
                        break;
                    case rrd_info_type_t.RD_I_CNT:
                        dict.Add(info.Value.key, info.Value.value.u_cnt);
                        break;
                    case rrd_info_type_t.RD_I_VAL:
                        dict.Add(info.Value.key, info.Value.value.u_val);
                        break;
                    case rrd_info_type_t.RD_I_BLO:
                        //TODO: Properly extract the byte array
                        dict.Add(info.Value.key, "[BLOB]");
                        break;
                }

                if (info.Value.next != IntPtr.Zero)
                    info = (rrd_info_t)System.Runtime.InteropServices.Marshal.PtrToStructure(info.Value.next, typeof(rrd_info_t));
                else
                    info = null;
            }
            return dict;
        }

        /// <summary>
        /// Returns a linked list of rrd_info_t objects that describe the rrd file. 
        /// </summary>
        /// <param name="filename">Full path to the rrd file</param>
        /// <returns>An rrd_info_t object</returns>
        public static Dictionary<string, object> Info(string filename)
        {
            if (string.IsNullOrEmpty(filename))
                throw new Exception("Empty filename");
            IntPtr ptr = rrd_info_r(filename);
            if (ptr == IntPtr.Zero || ptr.ToInt64() == -1)
                throw new RrdException();

            var ret = ConvertInfoToDict(ptr);
            rrd_info_free(ptr);
            return ret;
        }

        /// <summary>
        /// The update function feeds new data values into an RRD. The data is time aligned (interpolated) 
        /// according to the properties of the RRD to which the data is written.
        /// </summary>
        /// <param name="argv">String array of command line arguments</param>
        /// <returns>0 if successful, -1 if an error occurred</returns>
        public static void Update(params string[] argv)
        {
            if (rrd_update(argv.Length, argv) < 0)
            {
                throw new RrdException();
            }
        }

        /// <summary>
        /// The update function feeds new data values into an RRD. The data is time aligned (interpolated) 
        /// according to the properties of the RRD to which the data is written.
        /// </summary>
        /// <param name="argv">String array of command line arguments</param>
        /// <returns>An rrd_info_t pointer with information about the update</returns>
        public static IntPtr Update2(params string[] argv)
        {
            return rrd_update_v(argv.Length, argv);
        }

        /// <summary>
        /// The update function feeds new data values into an RRD. The data is time aligned (interpolated) 
        /// according to the properties of the RRD to which the data is written.
        /// </summary>
        /// <param name="filename">Full path to the rrd to update</param>
        /// <param name="template">List of data sources to update and in which order</param>
        /// <param name="argv">String array of command line arguments</param>
        public static void Update(string filename, string template, params string[] argv)
        {
            if (rrd_update_r(filename, template, argv.Length, argv) < 0)
            {
                throw new RrdException();
            }
        }

        /// <summary>
        /// Generate a graph from an RRD file. Specify all the graph options in the string array as you
        /// normally would with the command line version.
        /// </summary>
        /// <param name="argv">String array of command line arguments</param>
        /// <returns>0 if successful, -1 if an error occurred</returns>
        public static Dictionary<string, object> Graph(params string[] argv)
        {
            IntPtr ptr = rrd_graph_v(argv.Length, argv);

            if (ptr == IntPtr.Zero || ptr.ToInt64() == -1)
                throw new RrdException();

            var ret = ConvertInfoToDict(ptr);
            rrd_info_free(ptr);
            return ret;
        }

        /// <summary>
        /// Returns an array of values for the period specified from a given rrd.
        /// Specify your parameters in the argv array and check the referenced parameters for
        /// values returned from the rrd
        /// </summary>
        /// <param name="argv">String array of command line arguments (must include the filename)</param>
        /// <param name="start">Starting timestamp found in the rrd</param>
        /// <param name="end">Ending timestamp found in the rrd</param>
        /// <param name="step">The rrd's step value</param>
        /// <param name="ds_cnt">Number of data sources found</param>
        /// <param name="ds_namv">Names of data sources found</param>
        /// <param name="data">Values found (in double type)</param>
        public static void Fetch(string[] argv, ref DateTime start, ref DateTime end, ref UInt32 step,
            ref UInt32 ds_cnt, ref string[] ds_namv, ref IntPtr data)
        {
            Int64 starti64 = 0, endi64 = 0;
            IntPtr ptr = new IntPtr();
            if (rrd_fetch(argv.Length, argv, ref starti64, ref endi64, ref step, out ds_cnt,
                out ptr, out data) < 0)
            {
                throw new RrdException();
            }
            ds_namv = GetStringArray(ptr, ds_cnt);
            start = UnixTimestampToDateTime((int)starti64);
            end = UnixTimestampToDateTime((int)endi64);
        }

        /// <summary>
        /// Returns the timestamp of the first value in the rrd given the rra index 
        /// </summary>
        /// <param name="filename">Full path to the rrd file</param>
        /// <param name="rraindex">0 based index of the rra to get a value for</param>
        /// <returns>Unix timestamp if successful, -1 if an error occurred</returns>
        public static Int32 First(string filename, int rraindex)
        {
            Int32 rv = rrd_first_r(filename, rraindex);
            if (rv < 0)
            {
                throw new RrdException();
            }
            return rv;
        }

        /// <summary>
        /// Returns the timestamp of the first value in the rrd
        /// </summary>
        /// <param name="argv">String array of command line arguments</param>
        /// <returns>Unix timestamp if successful, -1 if an error occurred</returns>
        public static Int32 First(params string[] argv)
        {
            Int32 rv = rrd_first(argv.Length, argv);
            if (rv < 0)
            {
                throw new RrdException();
            }
            return rv;
        }

        /// <summary>
        /// Returns the timestamp of the last value in the rrd given the rra index
        /// </summary>
        /// <param name="filename"></param>
        /// <param name="filename">Full path to the rrd file</param>
        /// <param name="rraindex">0 based index of the rra to get a value for</param>
        /// <returns>Unix timestamp if successful, -1 if an error occurred</returns>
        public static DateTime Last(string filename, int rraindex)
        {
            Int32 rv = rrd_last_r(filename, rraindex);
            if (rv < 0)
            {
                throw new RrdException();
            }
            return UnixTimestampToDateTime(rv);
        }

        /// <summary>
        /// Returns the timestamp of the last value in the rrd
        /// </summary>
        /// <param name="argv">String array of command line arguments</param>
        /// <returns>Unix timestamp if successful, -1 if an error occurred</returns>
        public static DateTime Last(params string[] argv)
        {
            Int32 rv = rrd_last(argv.Length, argv);
            if (rv < 0)
            {
                throw new RrdException();
            }
            return UnixTimestampToDateTime(rv);
        }

        /// <summary>
        /// Finds the timestamp of the last updated value in the rrd
        /// </summary>
        /// <param name="filename">Full path to the rrd file</param>
        /// <param name="ret_last_update">Unix timestamp of the last update</param>
        /// <param name="ret_ds_count">Number of data sources found</param>
        /// <param name="ret_ds_names">Names of the data sources found</param>
        /// <param name="ret_last_ds">Name of the last data source found</param>
        public static void Last_Update(string filename, ref DateTime ret_last_update, ref UInt32 ret_ds_count,
            ref string[] ret_ds_names, ref string[] ret_last_ds)
        {
            IntPtr ds_names = new IntPtr();
            IntPtr last_ds = new IntPtr();
            Int32 last_update = 0;
            Int32 rt = rrd_lastupdate_r(filename, ref last_update, ref ret_ds_count, out ds_names, out last_ds);
            if (rt < 0)
            {
                throw new RrdException();
            }
            ret_last_update = UnixTimestampToDateTime(last_update);
            ret_ds_names = GetStringArray(ds_names, ret_ds_count);
            ret_last_ds = GetStringArray(last_ds, 1);
        }

        /// <summary>
        /// Writes the contents of an rrd file to an XML file
        /// </summary>
        /// <param name="filename">Full path to the rrd file</param>
        /// <param name="outname">Full path to write the XML output</param>
        /// <returns>0 if successful, -1 if an error occurred</returns>
        public static void Dump(string filename, string outname)
        {
            if (rrd_dump_r(filename, outname) < 0)
            {
                throw new RrdException();
            }
        }

        /// <summary>
        /// Writes the contents of an rrd file to an XML file
        /// </summary>
        /// <param name="argv">String array of command line arguments</param>
        /// <returns>0 if successful, -1 if an error occurred</returns>
        public static void Dump(params string[] argv)
        {
            if (rrd_dump(argv.Length, argv) < 0)
            {
                throw new RrdException();
            }
        }

        /// <summary>
        /// Grabs the values from an rrd. Similar to fetch but enables merging of multiple
        /// rrds and calculations
        /// </summary>
        /// <param name="argv">String array of command line arguments</param>
        /// <param name="start">Starting timestamp found in the rrd</param>
        /// <param name="end">Ending timestamp found in the rrd</param>
        /// <param name="step">Step size found in the rrd</param>
        /// <param name="col_cnt">Number of data sources found in the rrd</param>
        /// <param name="leggend_v">Add a legend</param>
        /// <param name="data">Values from the rrd as double type</param>
        public static void Xport(string[] argv, ref DateTime start, ref DateTime end, ref UInt32 step,
            ref UInt32 col_cnt, ref string[] legend_v, ref IntPtr data)
        {
            Int64 starti64 = 0, endi64 = 0;
            IntPtr legend = new IntPtr();
            Int32 rt = rrd_xport(argv.Length, argv, 0, ref starti64, ref endi64, ref step, ref col_cnt,
                out legend, out data);
            if (rt < 0)
            {
                throw new RrdException();
            }
            legend_v = GetStringArray(legend, col_cnt);
            start = UnixTimestampToDateTime((int)starti64);
            end = UnixTimestampToDateTime((int)endi64);
        }

        /// <summary>
        /// Creates an rrd from an XML data dump
        /// </summary>
        /// <param name="argv">String array of command line arguments</param>
        /// <returns>0 if successful, -1 if an error occurred</returns>
        public static void Restore(params string[] argv)
        {
            if (rrd_restore(argv.Length, argv) < 0)
            {
                throw new RrdException();
            }
        }

        /// <summary>
        /// Alters the size of an RRA and creates a new rrd in the dll's directory
        /// NOTE: The new rrd may return unexpected results if you are not very careful
        /// NOTE: This may crash in version 1.4.3
        /// </summary>
        /// <param name="argv">String array of command line arguments</param>
        public static void Resize(params string[] argv)
        {
            if (rrd_resize(argv.Length, argv) < 0)
            {
                throw new RrdException();
            }
        }

        /// <summary>
        /// Modify the characteristics of an rrd
        /// </summary>
        /// <param name="argv">String array of command line arguments</param>
        public static void Tune(params string[] argv)
        {
            if (rrd_tune(argv.Length, argv) < 0)
            {
                throw new RrdException();
            }
        }

        // UTILITIES
        /// <summary>
        /// Returns a string with the numeric version of the rrdlib build version
        /// </summary>
        /// <returns>A string with version information</returns>
        public static string Version()
        {
            return rrd_strversion();
        }

        /// <summary>
        /// Generates a random number for testing rrdlib
        /// </summary>
        /// <returns>A random integer</returns>
        public static int Random()
        {
            return rrd_random();
        }

        /// <summary>
        /// Returns the latest error from rrdlib
        /// </summary>
        /// <returns>A string with the error message, or an empty string if no error occurred</returns>
        public static string Get_Error()
        {
            IntPtr ptr = rrd_get_error();
            if (ptr == IntPtr.Zero)
                return "";
            return Marshal.PtrToStringAnsi(ptr);
        }

        /// <summary>
        /// Formats and prints information in the object to the standard output
        /// </summary>
        /// <param name="info">rrd_info_t object with data to print</param>
        public static void Info_Print(string filename)
        {
            if (string.IsNullOrEmpty(filename))
                throw new Exception("Empty filename");
            IntPtr ptr = rrd_info_r(filename);
            if (ptr == IntPtr.Zero || ptr.ToInt64() == -1)
                throw new RrdException();

            var data = (rrd_info_t)Marshal.PtrToStructure(ptr, typeof(rrd_info_t));

            rrd_info_print(ptr);
            rrd_info_free(ptr);
        }

        /// <summary>
        /// Converts a Char ** array of characters from the RRDLib returned as an IntPtr and converts
        /// it to a String array given the number of items in the ptr array.
        /// Re: http://stackoverflow.com/questions/1498931/marshalling-array-of-strings-to-char-in-c-must-be-quite-easy-if-you-know-ho
        /// </summary>
        /// <param name="ptr">Pointer to a character array returned from the RRDLib</param>
        /// <param name="size">Number of items in the character array (not the number of characters)</param>
        /// <returns>A string array</returns>
        private static string[] GetStringArray(IntPtr ptr, UInt32 size)
        {
            var list = new List<string>();
            for (int i = 0; i < size; i++)
            {
                var strPtr = (IntPtr)Marshal.PtrToStructure(ptr, typeof(IntPtr));
                list.Add(Marshal.PtrToStringAnsi(strPtr));
                ptr = new IntPtr(ptr.ToInt64() + IntPtr.Size);
            }
            return list.ToArray();
        }
    }
}
