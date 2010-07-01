/*****************************************************************************
 * RRDLIB .NET Binding Test
 *****************************************************************************
 * Created 2010/06/29 by Chris Larsen
 * 
 * This project tests the .NET binding library by creating an rrd, inserting 
 * data, fetching data, creating graphs, dumping and exporting the data to
 * XML, then restoring from an XML file. The examples follow the tutorial 
 * written by Alex van den Bogaerdt found at 
 * http://oss.oetiker.ch/rrdtool/tut/rrdtutorial.en.html
 ****************************************************************************/

using System;
using System.Collections;
using System.Runtime.InteropServices;
using dnrrdlib;

namespace dnrrd_binding_test
{
    class rrd_binding_test
    {
        private static string path = "";

        static void Main(string[] args)
        {
            Console.WriteLine("----- Starting Tests -----");
            Console.WriteLine("RRDLib Version: " + rrd.Version());

            Test_Create();
            Test_Get_Info();
            Test_Update();
            Test_Fetch();
            Test_Graph();
            Test_Graph_Math();
            Test_Graph_Math2();
            Test_Graph_Math3();
            Test_First_Last();
            Test_Dump();
            Test_Xport();
            Test_Restore();
            Test_Tune();
            Console.WriteLine("\n!!!!!! Finished !!!!!!!");
            string inp = Console.ReadLine();
        }

        static void Test_Create()
        {
            // create
            ArrayList al = new ArrayList();
            al.Add("DS:speed:COUNTER:600:U:U");
            al.Add("RRA:AVERAGE:0.5:1:24");
            al.Add("RRA:AVERAGE:0.5:6:10");

            int ret = rrd.Create(path + "test_a.rrd", 300, 920804400, (string[])al.ToArray(typeof(string)));
            if (ret < 0)
                Console.WriteLine("Error: " + rrd.Get_Error());
            else
                Console.WriteLine("Test create: Successful!");
        }
        static void Test_Get_Info()
        {
            Console.WriteLine("Try getting info...");
            rrd_info_t? info = rrd.Info(path + "test_a.rrd");
            if (info == null)
            {
                Console.WriteLine("Error: " + rrd.Get_Error());
                return;
            }
            while (info != null)
            {
                //info = (rrd_info_t)Marshal.PtrToStructure(info.next, typeof(rrd_info_t));
                Console.Write(((rrd_info_t)info).key + ": ");
                switch (((rrd_info_t)info).type)
                {
                    case rrd_info_type_t.RD_I_STR:
                        Console.WriteLine("\"" + Marshal.PtrToStringAnsi(((rrd_info_t)info).value.u_str) + "\"");
                        break;
                    case rrd_info_type_t.RD_I_INT:
                        Console.WriteLine(((rrd_info_t)info).value.u_int);
                        break;
                    case rrd_info_type_t.RD_I_CNT:
                        Console.WriteLine(((rrd_info_t)info).value.u_cnt);
                        break;
                    case rrd_info_type_t.RD_I_VAL:
                        Console.WriteLine(((rrd_info_t)info).value.u_val);
                        break;
                    case rrd_info_type_t.RD_I_BLO:
                        Console.WriteLine(" ** BLOB ** ");
                        break;
                    default:
                        Console.WriteLine("**** Unknown type! ****");
                        break;
                }

                if (((rrd_info_t)info).next != IntPtr.Zero && (int)((rrd_info_t)info).next > 0)
                    info = (rrd_info_t)Marshal.PtrToStructure(((rrd_info_t)info).next, typeof(rrd_info_t));
                else
                    info = null;
            }
            Console.WriteLine("Test Info: Successful!");
            //Console.WriteLine("Printing information...");
            //Info_Print(((rrd_info_t)info));
        }
        static void Test_Update()
        {
            Console.WriteLine("Updating RRD...");
            ArrayList al = new ArrayList();

            // set to false if you want to use random values
            if (true)
            {
                al.Add("920804700:12345");
                al.Add("920805000:12357");
                al.Add("920805300:12363");
                al.Add("920805600:12363");
                al.Add("920805900:12363");
                al.Add("920806200:12373");
                al.Add("920806500:12383");
                al.Add("920806800:12393");
                al.Add("920807100:12399");
                al.Add("920807400:12405");
                al.Add("920807700:12411");
                al.Add("920808000:12415");
                al.Add("920808300:12420");
                al.Add("920808600:12422");
                al.Add("920808900:12423");
            }
            else
            {
                UInt32 ts = 920804700;
                for (int i = 0; i < 15; i++)
                {
                    al.Add(ts.ToString() + ":" + rrd.Random());
                    ts += 300;
                }
            }
            int ret = rrd.Update(path + "test_a.rrd", null, (string[])al.ToArray(typeof(string)));
            if (ret < 0)
                Console.WriteLine("Error: " + rrd.Get_Error());
            else
                Console.WriteLine("Test update: Successful!");
        }
        static void Test_Fetch()
        {
            // FETCH
            Console.WriteLine("Attempting Fetch...");
            ArrayList al = new ArrayList();
            al.Add("fetch");
            al.Add(path + "test_a.rrd");
            al.Add("AVERAGE");
            al.Add("--start");
            al.Add("920804400");
            al.Add("--end");
            al.Add("920809200");
            IntPtr data = new IntPtr();
            string[] rrds = new string[0];
            Int32 start = 0;
            Int32 end = 0;
            UInt32 step = 0;
            UInt32 dscnt = 0;
            int ret = rrd.Fetch((string[])al.ToArray(typeof(string)), ref start, ref end,
                ref step, ref dscnt, ref rrds, ref data);

            if (end > start)
            {
                for (Int32 ti = start + (Int32)step; ti <= end; ti += (Int32)step)
                {
                    Console.Write(ti + ": ");
                    for (Int32 i = 0; i < (Int32)dscnt; i++)
                    {
                        Console.Write(((double)Marshal.PtrToStructure(data, typeof(double))).ToString(" 0.0000000000e+00"));
                        data = new IntPtr(data.ToInt64() + sizeof(double));
                    }
                    Console.Write(Environment.NewLine);
                }
            }

            if (ret < 0)
                Console.WriteLine("Error: " + rrd.Get_Error());
            else
                Console.WriteLine("Test fetch: Successful!");
        }
        static void Test_Graph()
        {
            Console.WriteLine("Creating graph...");
            ArrayList al = new ArrayList();
            al.Add("graph");
            al.Add(path + "graph_simple.png");
            al.Add("--start");
            al.Add("920804400");
            al.Add("--end");
            al.Add("920808000");
            al.Add("DEF:myspeed=" + path.Replace(":", "\\:") + "test_a.rrd:speed:AVERAGE");
            al.Add("LINE2:myspeed#00004D");
            int ret = rrd.Graph((string[])al.ToArray(typeof(string)));
            if (ret < 0)
                Console.WriteLine("Error: " + rrd.Get_Error());
            else
                Console.WriteLine("Test graph: Successful!");
        }
        static void Test_Graph_Math()
        {
            Console.WriteLine("Creating graph...");
            ArrayList al = new ArrayList();
            al.Add("graph");
            al.Add(path + "graph_math.png");
            al.Add("--start");
            al.Add("920804400");
            al.Add("--end");
            al.Add("920808000");
            al.Add("--vertical-label");
            al.Add("m/s");
            al.Add("DEF:myspeed=" + path.Replace(":", "\\:") + "test_a.rrd:speed:AVERAGE");
            al.Add("CDEF:realspeed=myspeed,1000,*");
            al.Add("LINE2:realspeed#00004D");
            int ret = rrd.Graph((string[])al.ToArray(typeof(string)));
            if (ret < 0)
                Console.WriteLine("Error: " + rrd.Get_Error());
            else
                Console.WriteLine("Test graph: Successful!");
        }
        static void Test_Graph_Math2()
        {
            Console.WriteLine("Creating graph...");
            ArrayList al = new ArrayList();
            al.Add("graph");
            al.Add(path + "graph_math2.png");
            al.Add("--start");
            al.Add("920804400");
            al.Add("--end");
            al.Add("920808000");
            al.Add("--vertical-label");
            al.Add("m/s");
            al.Add("DEF:myspeed=" + path.Replace(":", "\\:") + "test_a.rrd:speed:AVERAGE");
            al.Add("CDEF:kmh=myspeed,3600,*");
            al.Add("CDEF:fast=kmh,100,GT,kmh,0,IF");
            al.Add("CDEF:good=kmh,100,GT,0,kmh,IF");
            al.Add("HRULE:100#0000FF:\"Maximum allowed\"");
            al.Add("AREA:good#00FF00:\"Good speed\"");
            al.Add("AREA:fast#FF0000:\"Too fast\"");
            int ret = rrd.Graph((string[])al.ToArray(typeof(string)));
            if (ret < 0)
                Console.WriteLine("Error: " + rrd.Get_Error());
            else
                Console.WriteLine("Test graph: Successful!");
        }
        static void Test_Graph_Math3()
        {
            Console.WriteLine("Creating graph...");
            ArrayList al = new ArrayList();
            al.Add("graph");
            al.Add(path + "graph_math3.png");
            al.Add("--start");
            al.Add("920804400");
            al.Add("--end");
            al.Add("920808000");
            al.Add("--vertical-label");
            al.Add("m/s");
            al.Add("DEF:myspeed=" + path.Replace(":", "\\:") + "test_a.rrd:speed:AVERAGE");
            al.Add("CDEF:nonans=myspeed,UN,0,myspeed,IF");
            al.Add("CDEF:kmh=nonans,3600,*");
            al.Add("CDEF:fast=kmh,100,GT,100,0,IF");
            al.Add("CDEF:over=kmh,100,GT,kmh,100,-,0,IF");
            al.Add("CDEF:good=kmh,100,GT,0,kmh,IF");
            al.Add("HRULE:100#0000FF:\"Maximum allowed\"");
            al.Add("AREA:good#00FF00:\"Good speed\"");
            al.Add("AREA:fast#550000:\"Too fast\"");
            al.Add("STACK:over#FF0000:\"Over speed\"");
            int ret = rrd.Graph((string[])al.ToArray(typeof(string)));
            if (ret < 0)
                Console.WriteLine("Error: " + rrd.Get_Error());
            else
                Console.WriteLine("Test graph: Successful!");
        }
        static void Test_First_Last()
        {
            Console.WriteLine("Testing values...");
            Console.WriteLine("First Value: " + rrd.First(path + "test_a.rrd", 0));
            string err = rrd.Get_Error();
            if (err.Length > 1)
                Console.WriteLine("Error: " + err);
            Console.WriteLine("Last Value: " + rrd.Last(path + "test_a.rrd", 0));
            err = rrd.Get_Error();
            if (err.Length > 1)
                Console.WriteLine("Error: " + err);

            Int32 last_update = 0;
            UInt32 ds_count = 0;
            string[] ds_names = new string[0];
            string[] last_ds = new string[0];
            rrd.Last_Update(path + "test_a.rrd", ref last_update, ref ds_count, ref ds_names, ref last_ds);
            Console.WriteLine("Last Update: " + last_update);
            Console.WriteLine("Value testing successful!");
        }
        static void Test_Dump()
        {
            Console.WriteLine("Dumping RRD...");
            int ret = rrd.Dump(path + "test_a.rrd", path + "test_a.xml");
            if (ret < 0)
                Console.WriteLine("Error: " + rrd.Get_Error());
            else
                Console.WriteLine("Test Dump: Successful!");
        }
        static void Test_Xport()
        {
            Console.WriteLine("Exporting RRD...");
            ArrayList al = new ArrayList();
            al.Add("xport");
            al.Add("--start");
            al.Add("920804400");
            al.Add("--end");
            al.Add("920808000");
            al.Add("DEF:myspeed=" + path.Replace(":", "\\:") + "test_a.rrd:speed:AVERAGE");
            al.Add("XPORT:myspeed:\"MySpeed\"");
            IntPtr data = new IntPtr();
            string[] legends = new string[0];
            Int32 start = 0;
            Int32 end = 0;
            UInt32 step = 0;
            UInt32 col_cnt = 0;
            int ret = rrd.Xport((string[])al.ToArray(typeof(string)), ref start, ref end,
                ref step, ref col_cnt, ref legends, ref data);

            if (end > start)
            {
                for (Int32 ti = start + (Int32)step; ti <= end; ti += (Int32)step)
                {
                    Console.Write(ti + ": ");
                    for (Int32 i = 0; i < (Int32)col_cnt; i++)
                    {
                        Console.Write(((double)Marshal.PtrToStructure(data, typeof(double))).ToString(" 0.0000000000e+00"));
                        data = new IntPtr(data.ToInt64() + sizeof(double));
                    }
                    Console.Write(Environment.NewLine);
                }
            }
            if (ret < 0)
                Console.WriteLine("Error: " + rrd.Get_Error());
            else
                Console.WriteLine("Test xport: Successful!");
        }
        static void Test_Restore()
        {
            Console.WriteLine("Restoring RRD...");
            ArrayList al = new ArrayList();
            al.Add("restore");
            al.Add(path + "test_a.xml");
            al.Add(path + "restored_a.rrd");
            int ret = rrd.Restore((string[])al.ToArray(typeof(string)));
            if (ret < 0)
                Console.WriteLine("Error: " + rrd.Get_Error());
            else
                Console.WriteLine("Test restore: Successful!");
        }
        static void Test_Tune()
        {
            Console.WriteLine("Tuning RRD...");
            ArrayList al = new ArrayList();
            al.Add("tune");
            al.Add(path + "restored_a.rrd");
            al.Add("-h");
            al.Add("speed:650");
            int ret = rrd.Tune((string[])al.ToArray(typeof(string)));
            if (ret < 0)
                Console.WriteLine("Error: " + rrd.Get_Error());
            else
                Console.WriteLine("Test tune: Successful!");
        }
    }
}
