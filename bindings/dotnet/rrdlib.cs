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
 * For useage examples, please see the rrd_binding_test project.
 ****************************************************************************/
using System;
using System.Runtime.InteropServices;

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
    /// type field to deteremine which of these to read. 
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
        const string dll = @"rrdlib.dll";

        // IMPORTS - Main methods
        [DllImport(dll)] static extern Int32 rrd_create(Int32 argc, string[] argv);
        [DllImport(dll)] static extern Int32 rrd_create_r([MarshalAs(UnmanagedType.LPStr)] string filename, 
            UInt32 pdp_step, Int32 last_up, Int32 argc, [MarshalAs(UnmanagedType.LPArray)] string[] argv);
        [DllImport(dll)] static extern IntPtr rrd_info_r(string filename);
        [DllImport(dll)] static extern void rrd_info_print(IntPtr data);
        [DllImport(dll)] static extern Int32 rrd_update(Int32 argc, string[] argv);
        [DllImport(dll)] static extern IntPtr rrd_update_v(Int32 argc, string[] argv);
        [DllImport(dll)] static extern Int32 rrd_update_r(string filename, string template, Int32 argc,
            string[] argv);
        /* Do not use this until someone adds the FILE structure */
        [DllImport(dll)] static extern Int32 rrd_graph(Int32 argc, string[] argv, ref string[] prdata,
            ref Int32 xsize, ref Int32 ysize, /* TODO - FILE, */ ref double ymin, ref double ymax);
        [DllImport(dll)] static extern Int32 rrd_graph_v(Int32 argc, string[] argv);
        [DllImport(dll)] static extern Int32 rrd_fetch(Int32 argc, string[] argv, ref Int32 start,
            ref Int32 end, ref UInt32 step, ref UInt32 ds_cnt, ref string[] ds_namv, ref IntPtr data);
        [DllImport(dll)] static extern Int32 rrd_first(Int32 argc, string[] argv);
        [DllImport(dll)] static extern Int32 rrd_first_r(string filename, Int32 rraindex);
        [DllImport(dll)] static extern Int32 rrd_last(Int32 argc, string[] argv);
        [DllImport(dll)] static extern Int32 rrd_last_r(string filename, Int32 rraindex);
        [DllImport(dll)] static extern Int32 rrd_lastupdate(Int32 argc, string[] argv);
        [DllImport(dll)] static extern Int32 rrd_lastupdate_r(string filename, ref Int32 ret_last_update,
            ref UInt32 ret_ds_count, ref string[] ret_ds_names, ref string[] ret_last_ds);
        [DllImport(dll)] static extern Int32 rrd_dump(Int32 argc, string[] argv);
        [DllImport(dll)] static extern Int32 rrd_dump_r(string filename, string outname);
        [DllImport(dll)] static extern Int32 rrd_xport(Int32 argc, string[] argv, Int32 unused,
            ref Int32 start, ref Int32 end, ref UInt32 step, ref UInt32 col_cnt,
            ref string[] leggend_v, ref IntPtr data);
        [DllImport(dll)] static extern Int32 rrd_restore(Int32 argc, string[] argv);
        [DllImport(dll)] static extern Int32 rrd_resize(Int32 argc, string[] argv);
        [DllImport(dll)] static extern Int32 rrd_tune(Int32 argc, string[] argv);

        // IMPORTS - Utility methods
        [DllImport(dll)] static extern string rrd_strversion();
        [DllImport(dll)] static extern Int32 rrd_random();
        [DllImport(dll)] static extern string rrd_get_error();

        // MAIN FUNCTIONS

        /// <summary>
        /// The create function of RRDtool lets you set up new Round Robin Database (RRD) files. 
        /// The file is created at its final, full size and filled with *UNKNOWN* data.
        /// </summary>
        /// <param name="argv">String array of command line arguments</param>
        /// <returns>0 if successful, -1 if an error occurred</returns>
        public static int Create(string[] argv)
        {
            return rrd_create(argv.GetUpperBound(0) + 1, argv);
        }

        /// <summary>
        /// The create function of RRDtool lets you set up new Round Robin Database (RRD) files. 
        /// The file is created at its final, full size and filled with *UNKNOWN* data.
        /// </summary>
        /// <param name="filename">A full path to the location where you want the rrd to reside</param>
        /// <param name="pdp_step">Specifies the base interval in seconds with which data will be fed into the RRD</param>
        /// <param name="last_up">Timestamp of the last update</param>
        /// <param name="argv">String array of command line arguments</param>
        /// <returns>0 if successful, -1 if an error occurred</returns>
        public static int Create(string filename, UInt32 pdp_step, Int32 last_up, string[] argv)
        {
            return rrd_create_r(filename, pdp_step, last_up, argv.GetUpperBound(0)+1, argv);
        }

        /// <summary>
        /// Returns a linked list of rrd_info_t objects that describe the rrd file. 
        /// </summary>
        /// <param name="filename">Full path to the rrd file</param>
        /// <returns>An rrd_info_t object</returns>
        public static rrd_info_t Info(string filename)
        {
            if (filename.Length < 1)
                throw new Exception("Empty filename");
            IntPtr ptr = rrd_info_r(filename);
            if (ptr == IntPtr.Zero || (int)ptr < 1)
                throw new Exception("Unable to extract information from rrd");
            return (rrd_info_t)Marshal.PtrToStructure(ptr, typeof(rrd_info_t));
        }

        /// <summary>
        /// The update function feeds new data values into an RRD. The data is time aligned (interpolated) 
        /// according to the properties of the RRD to which the data is written.
        /// </summary>
        /// <param name="argv">String array of command line arguments</param>
        /// <returns>0 if successful, -1 if an error occurred</returns>
        public static Int32 Update(string[] argv)
        {
            return rrd_update(argv.GetUpperBound(0) + 1, argv);
        }

        /// <summary>
        /// The update function feeds new data values into an RRD. The data is time aligned (interpolated) 
        /// according to the properties of the RRD to which the data is written.
        /// </summary>
        /// <param name="argv">String array of command line arguments</param>
        /// <returns>An rrd_info_t pointer with information about the update</returns>
        public static IntPtr Update2(string[] argv)
        {
            return rrd_update_v(argv.GetUpperBound(0) + 1, argv);
        }

        /// <summary>
        /// The update function feeds new data values into an RRD. The data is time aligned (interpolated) 
        /// according to the properties of the RRD to which the data is written.
        /// </summary>
        /// <param name="filename">Full path to the rrd to update</param>
        /// <param name="template">List of data sources to update and in which order</param>
        /// <param name="argv">String array of command line arguments</param>
        /// <returns>0 if successful, -1 if an error occurred</returns>
        public static Int32 Update(string filename, string template, string[] argv)
        {
            return rrd_update_r(filename, template, argv.GetUpperBound(0)+1, argv);
        }

        /// <summary>
        /// Generate a graph from an RRD file. Specify all the graph options in the string array as you
        /// normally would with the command line version.
        /// </summary>
        /// <param name="argv">String array of command line arguments</param>
        /// <returns>0 if successful, -1 if an error occurred</returns>
        public static Int32 Graph(string[] argv)
        {
            return rrd_graph_v(argv.GetUpperBound(0) + 1, argv);
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
        /// <returns>0 if successful, -1 if an error occurred</returns>
        public static Int32 Fetch(string[] argv, ref Int32 start, ref Int32 end, ref UInt32 step,
            ref UInt32 ds_cnt, ref string[] ds_namv, ref IntPtr data)
        {
            return rrd_fetch(argv.GetUpperBound(0) + 1, argv, ref start, ref end, ref step, ref ds_cnt,
                ref ds_namv, ref data);
        }

        /// <summary>
        /// Returns the timestamp of the first value in the rrd given the rra index 
        /// </summary>
        /// <param name="filename">Full path to the rrd file</param>
        /// <param name="rraindex">0 based index of the rra to get a value for</param>
        /// <returns>Unix timestamp if successful, -1 if an error occurred</returns>
        public static Int32 First(string filename, int rraindex)
        {
            return rrd_first_r(filename, rraindex);
        }

        /// <summary>
        /// Returns the timestamp of the first value in the rrd
        /// </summary>
        /// <param name="argv">String array of command line arguments</param>
        /// <returns>Unix timestamp if successful, -1 if an error occurred</returns>
        public static Int32 First(string[] argv)
        {
            return rrd_first(argv.GetUpperBound(0) + 1, argv);
        }

        /// <summary>
        /// Returns the timestamp of the last value in the rrd given the rra index
        /// </summary>
        /// <param name="filename"></param>
        /// <param name="filename">Full path to the rrd file</param>
        /// <param name="rraindex">0 based index of the rra to get a value for</param>
        /// <returns>Unix timestamp if successful, -1 if an error occurred</returns>
        public static Int32 Last(string filename, int rraindex)
        {
            return rrd_last_r(filename, rraindex);
        }

        /// <summary>
        /// Returns the timestamp of the last value in the rrd
        /// </summary>
        /// <param name="argv">String array of command line arguments</param>
        /// <returns>Unix timestamp if successful, -1 if an error occurred</returns>
        public static Int32 Last(string[] argv)
        {
            return rrd_last(argv.GetUpperBound(0) + 1, argv);
        }

        /// <summary>
        /// Finds the timestamp of the last updated value in the rrd
        /// </summary>
        /// <param name="filename">Full path to the rrd file</param>
        /// <param name="ret_last_update">Unix timestamp of the last update</param>
        /// <param name="ret_ds_count">Number of data sources found</param>
        /// <param name="ret_ds_names">Names of the data sources found</param>
        /// <param name="ret_last_ds">Name of the last data source found</param>
        /// <returns>0 if successful, -1 if an error occurred</returns>
        public static Int32 Last_Update(string filename, ref Int32 ret_last_update, ref UInt32 ret_ds_count,
            ref string[] ret_ds_names, ref string[] ret_last_ds)
        {
            return rrd_lastupdate_r(filename, ref ret_last_update, ref ret_ds_count, ref ret_ds_names,
                ref ret_last_ds);
        }

        /// <summary>
        /// Writes the contents of an rrd file to an XML file
        /// </summary>
        /// <param name="filename">Full path to the rrd file</param>
        /// <param name="outname">Full path to write the XML output</param>
        /// <returns>0 if successful, -1 if an error occurred</returns>
        public static Int32 Dump(string filename, string outname)
        {
            return rrd_dump_r(filename, outname);
        }

        /// <summary>
        /// Writes the contents of an rrd file to an XML file
        /// </summary>
        /// <param name="argv">String array of command line arguments</param>
        /// <returns>0 if successful, -1 if an error occurred</returns>
        public static Int32 Dump(string[] argv)
        {
            return rrd_dump(argv.GetUpperBound(0) + 1, argv);
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
        /// <returns>0 if successful, -1 if an error occurred</returns>
        public static Int32 Xport(string[] argv, ref Int32 start, ref Int32 end, ref UInt32 step,
            ref UInt32 col_cnt, ref string[] leggend_v, ref IntPtr data)
        {
            return rrd_xport(argv.GetUpperBound(0) + 1, argv, 0, ref start, ref end, ref step, ref col_cnt,
                ref leggend_v, ref data);
        }

        /// <summary>
        /// Creates an rrd from an XML data dump
        /// </summary>
        /// <param name="argv">String array of command line arguments</param>
        /// <returns>0 if successful, -1 if an error occurred</returns>
        public static Int32 Restore(string[] argv)
        {
            return rrd_restore(argv.GetUpperBound(0) + 1, argv);
        }

        /// <summary>
        /// Alters the size of an RRA and creates a new rrd in the dll's directory
        /// NOTE: The new rrd may return unexpected results if you are not very careful
        /// NOTE: This may crash in version 1.4.3
        /// </summary>
        /// <param name="argv">String array of command line arguments</param>
        /// <returns>0 if successful, -1 if an error occurred</returns>
        public static Int32 Resize(string[] argv)
        {
            return rrd_resize(argv.GetUpperBound(0) + 1, argv);
        }

        /// <summary>
        /// Modify the characteristics of an rrd
        /// </summary>
        /// <param name="argv">String array of command line arguments</param>
        /// <returns>0 if successful, -1 if an error occurred</returns>
        public static Int32 Tune(string[] argv)
        {
            return rrd_tune(argv.GetUpperBound(0) + 1, argv);
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
        /// <returns>A string with the error message, or an emtpy string if no error occurred</returns>
        public static string Get_Error()
        {
            return rrd_get_error();
        }

        /// <summary>
        /// Formats and prints information in the object to the standard output
        /// </summary>
        /// <param name="info">rrd_info_t object with data to print</param>
        public static void Info_Print(rrd_info_t info)
        {
            IntPtr newptr = Marshal.AllocHGlobal(Marshal.SizeOf(info));
            Marshal.StructureToPtr(info, newptr, true);
            rrd_info_print(newptr);
        }
    }
}
