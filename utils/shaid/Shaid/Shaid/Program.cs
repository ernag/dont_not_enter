using System;
using System.Diagnostics;
using System.IO;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Shaid
{
    class Program
    {
        static void Main(string[] args)
        {
            StringBuilder sbLog = new StringBuilder();
            string projDir;
            int idx2;

            Process p = new Process();
            // Redirect the output stream of the child process.
            p.StartInfo.UseShellExecute = false;
            p.StartInfo.RedirectStandardOutput = true;
            string gitPath = Environment.GetEnvironmentVariable("ProgramFiles(x86)");
            p.StartInfo.FileName = gitPath + "\\Git\\cmd\\git.exe";
            sbLog.AppendFormat("gitPath = {0}\n", p.StartInfo.FileName);
            p.StartInfo.Arguments = "log -1 --format=\"%H\"";
            p.Start();
            string build_id = p.StandardOutput.ReadToEnd();
            p.WaitForExit();

            int idx = build_id.IndexOf("\n");
            build_id = build_id.Remove(idx);
            sbLog.AppendFormat("build_id = {0}\n", build_id);

            DateTime dt = DateTime.Now;
            string build_time = dt.ToString("yyyy/MM/dd HH:mm:ss");
            // string build_time = dt.ToString("yyyyMMddHHmmss");
            sbLog.AppendFormat("build_time = {0}\n", build_time);

            string currentDir = Environment.CurrentDirectory;
            sbLog.AppendFormat("CurrentDirectory = {0}\n", currentDir);

            idx2 = currentDir.IndexOf("MQX");   // in dir when called from CW build
            sbLog.AppendFormat("idx2 = {0}\n", idx2);
            if (idx2 < 0)
            {
                idx2 = currentDir.IndexOf("utils"); // in dir when executed via debug or direct
            }
            sbLog.AppendFormat("idx2 = {0}\n", idx2);
            if (idx2 < 0)
            {
                sbLog.AppendFormat("ERROR: UNKNOWN PATH: {0}", currentDir);
                Environment.Exit(0);
            }
            projDir = currentDir.Remove(idx2);
            sbLog.AppendFormat("projDir = {0}\n", projDir);

            string path = projDir + "MQX 3.8\\whistle\\corona\\include\\shaid.h";
            sbLog.AppendFormat("path = {0}\n", path);

            byte[] buffer;

            // Create the shaid.h file
            using (FileStream fs = File.OpenWrite(path))
            {
                StringBuilder fh = new StringBuilder();
                fh.Append("/* shaid.h */\n\n");
                fh.Append("#ifndef SHAID_H_\n");
                fh.Append("#define SHAID_H_\n");
                fh.Append("\n");
                fh.AppendFormat("const char* build_id   = \"{0}\";\n", build_id);
                fh.AppendFormat("const char* build_time = \"{0}\";\n", build_time);
                fh.Append("\n");
                fh.Append("#endif /* SHAID_H_ */\n");
                fh.Append("\n");
                fh.Append("////////////////////////////////////////////\n");

                string fhStr = fh.ToString();

                buffer = Encoding.ASCII.GetBytes(fh.ToString());
                fs.Write(buffer, 0, buffer.Length);
            }

            byte[] bufLog;
            string logPath = projDir + "utils\\shaid\\Shaid\\Shaid\\shaid.log";
            using (FileStream flog = File.OpenWrite(logPath))
            {
                bufLog = Encoding.ASCII.GetBytes(sbLog.ToString());
                flog.Write(bufLog, 0, bufLog.Length);
                flog.Write(buffer, 0, buffer.Length);
            }
        }
    }
}
