using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Drawing.Text;
using System.IO;
using System.Threading;
using System.Windows.Forms;

namespace PixelRacer
{
    internal static class Program
    {
        [STAThread]
        private static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new GameForm());
        }
    }

    /// <summary>
    /// 俯视视角的像素赛车：在低分辨率离屏位图上绘制，再以最近邻方式整数倍放大。
    /// 逻辑与 index.html 一一对应（函数名由 sync-config.js 校验），
    /// 所有可调参数来自 game-config.json 生成的 Config.cs。
    /// 语法保持 C# 5 兼容，既能用 VS/net8.0 打开，也能用 Windows 自带 csc 编译。
    /// </summary>
    internal sealed class GameForm : Form
    {
        private const int W = Config.W, H = Config.H;
        private const int RoadL = Config.RoadL, RoadR = Config.RoadR;
        private const int CarW = Config.CarW, CarH = Config.CarH;
        private const int PlayerY = Config.PlayerY, HudH = Config.HudH;
        private static readonly int[] Lanes = Config.Lanes;

        private const string CGrass = Config.Grass, CGrass2 = Config.Grass2;
        private const string CRumble1 = Config.Rumble1, CRumble2 = Config.Rumble2;
        private const string CRoad = Config.Road, CRoad2 = Config.Road2, CDash = Config.Dash;
        private const string CBlack = Config.Black, CGlass = Config.Glass, CWhite = Config.White;
        private const string CYellow = Config.Yellow, CRed = Config.Red, CPlayer = Config.Player;
        private const string CTree = Config.Tree, CTreeHi = Config.TreeHi;
        private const string CPole = Config.Pole, CSpeedLine = Config.SpeedLine, CWarn = Config.Warn;

        private enum St { Title, Play, Pause, Over }

        private sealed class Enemy
        {
            public int X; public double Y; public string Color; public bool Passed; public bool Near;
        }
        private sealed class Coin { public int X; public double Y; }
        private sealed class Part
        {
            public double X, Y, Vx, Vy, Life; public string Color; public int S;
        }
        private sealed class Floater
        {
            public string Text; public double X, Y, Life; public string Color;
        }
        private sealed class WavePlan
        {
            public List<int> Block; public int Coin;
            public WavePlan(List<int> block, int coin) { Block = block; Coin = coin; }
        }
        private sealed class WarnInfo
        {
            public double T; public List<int> Block; public int Coin;
        }
        private struct TextGlyph
        {
            public Bitmap Bmp; public int Width; public int Height;
        }

        private readonly Bitmap back = new Bitmap(W, H, PixelFormat.Format32bppPArgb);
        private Graphics g;
        private readonly Dictionary<string, SolidBrush> brushes = new Dictionary<string, SolidBrush>();
        private readonly Dictionary<string, string> shades = new Dictionary<string, string>();
        private readonly Dictionary<string, TextGlyph> textCache = new Dictionary<string, TextGlyph>();
        private readonly SolidBrush overlay = new SolidBrush(Color.FromArgb(225, 10, 12, 18));
        private readonly SolidBrush hudBg = new SolidBrush(Color.FromArgb(214, 12, 14, 20));
        private readonly SolidBrush flashBrush = new SolidBrush(Color.White);
        private readonly Font f6 = new Font("Consolas", 6f, FontStyle.Bold, GraphicsUnit.Pixel);
        private readonly Font f7 = new Font("Consolas", 7f, FontStyle.Bold, GraphicsUnit.Pixel);
        private readonly Font f8 = new Font("Consolas", 8f, FontStyle.Bold, GraphicsUnit.Pixel);
        private readonly Font f13 = new Font("Consolas", 13f, FontStyle.Bold, GraphicsUnit.Pixel);

        private readonly List<Enemy> enemies = new List<Enemy>();
        private readonly List<Coin> coins = new List<Coin>();
        private readonly List<Part> parts = new List<Part>();
        private readonly List<Floater> floaters = new List<Floater>();
        private readonly Queue<WavePlan> waveQueue = new Queue<WavePlan>();
        private readonly HashSet<Keys> held = new HashSet<Keys>();
        private readonly Random rnd = new Random();
        private readonly Stopwatch watch = new Stopwatch();
        private readonly Timer timer;

        private St state = St.Title;
        private bool left, right, muted, newRecord;
        private double score, scroll, speed, spawnT, coinT, runTime, time, shake, flash, hitStop;
        private double playerX, tilt, steerT, invuln, combo, comboT, engineT, acc;
        private int lives, laneIndex, stage, best;
        private WarnInfo warn;
        private int beepBusy;

        private static readonly string BestPath = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
            "PixelRacer", "best.txt");

        public GameForm()
        {
            Text = "Pixel Racer - 像素赛车";
            FormBorderStyle = FormBorderStyle.FixedSingle;
            MaximizeBox = false;
            StartPosition = FormStartPosition.CenterScreen;
            BackColor = Color.Black;
            DoubleBuffered = true;
            KeyPreview = true;

            int scale = Math.Max(2, Math.Min(3, (Screen.PrimaryScreen.WorkingArea.Height - 120) / H));
            ClientSize = new Size(W * scale, H * scale);

            g = Graphics.FromImage(back);
            g.InterpolationMode = InterpolationMode.NearestNeighbor;
            g.PixelOffsetMode = PixelOffsetMode.Half;
            g.SmoothingMode = SmoothingMode.None;
            g.TextRenderingHint = TextRenderingHint.SingleBitPerPixel;

            LoadBest();
            Reset();

            timer = new Timer { Interval = 16 };
            timer.Tick += delegate
            {
                double dt = watch.Elapsed.TotalSeconds;
                watch.Restart();
                if (dt > 0.25) dt = 0.25;
                acc += dt;
                int n = 0;
                while (acc >= Config.FixedStep && n < Config.MaxStepsPerFrame)
                {
                    Update(Config.FixedStep);
                    acc -= Config.FixedStep;
                    n++;
                }
                if (n == Config.MaxStepsPerFrame) acc = 0;
                Invalidate();
            };
            watch.Start();
            timer.Start();
        }

        // ---------- 资源与绘制辅助 ----------
        private Brush B(string html)
        {
            SolidBrush b;
            if (!brushes.TryGetValue(html, out b))
            {
                b = new SolidBrush(ColorTranslator.FromHtml(html));
                brushes[html] = b;
            }
            return b;
        }
        private void R(int x, int y, int w, int h, string c) { g.FillRectangle(B(c), x, y, w, h); }

        private string Shade(string hex, int amt)
        {
            string key = hex + amt;
            string cached;
            if (shades.TryGetValue(key, out cached)) return cached;
            int n = Convert.ToInt32(hex.Substring(1), 16);
            int r = Clamp((n >> 16) + amt, 0, 255);
            int gg = Clamp(((n >> 8) & 255) + amt, 0, 255);
            int b = Clamp((n & 255) + amt, 0, 255);
            string result = "#" + r.ToString("X2") + gg.ToString("X2") + b.ToString("X2");
            shades[key] = result;
            return result;
        }

        // 文字按「文本 + 颜色 + 字号」缓存为透明位图，避免每帧反复栅格化。
        private void T(string s, float x, float y, string color, Font font, StringAlignment align)
        {
            string key = s + "|" + color + "|" + font.Size.ToString("F0");
            TextGlyph tg;
            if (!textCache.TryGetValue(key, out tg))
            {
                float measured;
                using (Bitmap probe = new Bitmap(1, 1))
                using (Graphics pg = Graphics.FromImage(probe))
                    measured = pg.MeasureString(s, font, 999, StringFormat.GenericDefault).Width;
                int tw = Math.Min(W, (int)Math.Ceiling(measured) + 2);
                int th = Math.Min(28, (int)Math.Ceiling(font.Size) + 6);
                Bitmap bmp = new Bitmap(tw, th, PixelFormat.Format32bppPArgb);
                using (Graphics bmpG = Graphics.FromImage(bmp))
                using (StringFormat sf = new StringFormat())
                {
                    bmpG.TextRenderingHint = TextRenderingHint.SingleBitPerPixel;
                    bmpG.SmoothingMode = SmoothingMode.None;
                    sf.Alignment = StringAlignment.Near;
                    sf.LineAlignment = StringAlignment.Near;
                    bmpG.DrawString(s, font, B(color), 0, 0, sf);
                }
                tg = new TextGlyph { Bmp = bmp, Width = tw, Height = th };
                textCache[key] = tg;
            }
            float dx = x;
            if (align == StringAlignment.Center) dx = x - tg.Width / 2f;
            else if (align == StringAlignment.Far) dx = x - tg.Width;
            g.DrawImage(tg.Bmp, (int)Math.Round(dx), (int)Math.Round(y),
                        new Rectangle(0, 0, tg.Width, tg.Height), GraphicsUnit.Pixel);
        }

        private static int Clamp(int v, int a, int b) { return v < a ? a : (v > b ? b : v); }
        private static double Clamp(double v, double a, double b) { return v < a ? a : (v > b ? b : v); }
        private static bool Overlap(double ax, double ay, double aw, double ah,
                                    double bx, double by, double bw, double bh)
        {
            return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
        }
        private double Rnd() { return rnd.NextDouble(); }
        private int RndInt(int n) { return rnd.Next(n); }
        private static string Pad(int n) { return n.ToString("D5"); }

        // Console.Beep 是同步阻塞的，撞车时 80ms 足以掉帧；丢到线程池并做重叠节流。
        private void Beep(int freq, int ms)
        {
            if (Interlocked.Exchange(ref beepBusy, 1) != 0) return;
            ThreadPool.QueueUserWorkItem(delegate(object state)
            {
                try { Console.Beep(freq, ms); }
                catch (Exception) { /* 无声卡时忽略 */ }
                finally { Interlocked.Exchange(ref beepBusy, 0); }
            });
        }

        // ---------- 状态 ----------
        private void LoadBest()
        {
            try
            {
                if (File.Exists(BestPath)) int.TryParse(File.ReadAllText(BestPath), out best);
            }
            catch (Exception) { best = 0; }
        }
        private void SaveBest()
        {
            try
            {
                Directory.CreateDirectory(Path.GetDirectoryName(BestPath));
                File.WriteAllText(BestPath, best.ToString());
            }
            catch (Exception) { /* 无写权限时忽略 */ }
        }

        private void Reset()
        {
            score = 0; scroll = 0; speed = Config.SpeedBase;
            spawnT = 0.7; coinT = 1.6; runTime = 0; time = 0;
            shake = 0; flash = 0; hitStop = 0; newRecord = false;
            lives = Config.StartLives; invuln = 0; combo = 0; comboT = 0; stage = 0;
            laneIndex = 1; playerX = Lanes[1]; tilt = 0; steerT = 0; engineT = 0; acc = 0;
            enemies.Clear(); coins.Clear(); parts.Clear(); floaters.Clear();
            waveQueue.Clear(); warn = null;
        }
        private void Start()
        {
            Reset();
            state = St.Play;
            if (!muted) Beep(520, 40);
        }
        private void GameOver()
        {
            state = St.Over;
            Burst(playerX, PlayerY, CPlayer, 26);
            shake = 1;
            if (!muted) Beep(110, 80);
            if ((int)score > best) { best = (int)score; SaveBest(); newRecord = true; }
        }

        // ---------- 绘制 ----------
        // 车身按水平条带做整数像素错位，模拟转向倾斜且保持像素硬边。
        // C# 5 无局部函数，这里用三个字段传递绘制上下文。
        private int carX, carY;
        private double bandTilt;
        private void CR(int px, int py, int pw, int ph, string c)
        {
            int band = (int)Math.Round(bandTilt * (py / (double)CarH - 0.45));
            R(carX + px + band, carY + py, pw, ph, c);
        }
        private void DrawCar(double fx, double fy, string body, bool down, double bodyTilt)
        {
            carX = (int)fx; carY = (int)fy; bandTilt = bodyTilt;
            int w = CarW, h = CarH;
            string hi = Shade(body, 52), lo = Shade(body, -52);
            CR(-2, 4, 2, 5, CBlack); CR(w, 4, 2, 5, CBlack);
            CR(-2, 15, 2, 5, CBlack); CR(w, 15, 2, 5, CBlack);
            CR(0, 0, w, h, body);
            CR(1, 1, w - 2, 2, hi);
            CR(1, h - 2, w - 2, 2, lo);
            CR(2, 6, w - 4, 5, CGlass);
            CR(2, 15, w - 4, 4, CGlass);
            CR(2, 7, w - 4, 1, CWhite);
            CR(3, 12, w - 6, 1, hi);
            int headY = down ? h - 3 : 1;
            int tailY = down ? 1 : h - 3;
            CR(1, headY, 3, 2, CYellow); CR(w - 4, headY, 3, 2, CYellow);
            CR(1, tailY, 3, 2, CRed); CR(w - 4, tailY, 3, 2, CRed);
        }

        private void Tree(int x, int y)
        {
            R(x + 2, y + 7, 3, 5, CBlack);
            R(x, y + 1, 7, 7, CTree);
            R(x + 1, y + 2, 5, 3, CTreeHi);
        }
        private void Pole(int x, int y)
        {
            R(x + 1, y + 3, 2, 12, CBlack);
            R(x + 1, y + 3, 2, 10, CPole);
            R(x, y, 6, 3, CYellow);
            R(x + 1, y, 4, 1, CWhite);
        }
        private void DrawSceneryFar()
        {
            int step = 48, off = (int)(scroll * 0.35) % step;
            for (int y = -step + off; y < H; y += step) { Tree(4, y); Tree(146, y + 20); }
        }
        private void DrawSceneryNear()
        {
            int step = 80, off = (int)scroll % step;
            for (int y = -step + off; y < H; y += step) { Pole(10, y); Pole(146, y + 40); }
        }
        private void DrawSpeedLines()
        {
            if (speed < 140) return;
            int n = Math.Min(9, 2 + (int)((speed - 140) / 24));
            int off = (int)(scroll * 1.7);
            for (int i = 0; i < n; i++)
            {
                int x = RoadL + 6 + ((i * 47) % 108);
                int y = (int)((i * 61 + off * (1 + (i % 3) * 0.35)) % (H + 30)) - 20;
                R(x, y, 1, 5 + (i % 3) * 3, CSpeedLine);
            }
        }
        private void DrawRoad()
        {
            R(0, 0, W, H, CGrass);
            int stripe = 16, off = (int)(scroll % (stripe * 2));
            for (int y = -stripe * 2 + off; y < H; y += stripe * 2) R(0, y, W, stripe, CGrass2);
            DrawSceneryFar();

            int rb = 8, ro = (int)(scroll % (rb * 2));
            for (int y = -rb * 2 + ro; y < H; y += rb * 2)
            {
                R(RoadL - 4, y, 4, rb, CRumble1);
                R(RoadR, y, 4, rb, CRumble1);
                R(RoadL - 4, y + rb, 4, rb, CRumble2);
                R(RoadR, y + rb, 4, rb, CRumble2);
            }
            R(RoadL, 0, RoadR - RoadL, H, CRoad);
            int to = (int)(scroll % 24);
            for (int y = -24 + to; y < H; y += 24) R(RoadL, y, RoadR - RoadL, 1, CRoad2);
            int seg = 12, gap = 12, d = (int)(scroll % (seg + gap));
            for (int y = -(seg + gap) + d; y < H; y += seg + gap)
            {
                R(59, y, 2, seg, CDash);
                R(99, y, 2, seg, CDash);
            }
            DrawSceneryNear();
            DrawSpeedLines();
        }

        private void DrawCoin(int x, double y)
        {
            int w = (int)(3 + Math.Abs(Math.Sin(time * 5)) * 5);
            R((int)(x + (8 - w) / 2.0), (int)y, w, 8, CYellow);
            R((int)(x + (8 - w) / 2.0), (int)y, w, 2, CWhite);
        }
        private void DrawWarn()
        {
            if (warn == null || (int)Math.Floor(time * 14) % 2 != 0) return;
            foreach (int li in warn.Block)
            {
                int x = Lanes[li] + 4;
                R(x, HudH, 8, 3, CWarn);
                R(x + 2, HudH + 3, 4, 2, CWarn);
            }
        }
        private void DrawFloaters()
        {
            foreach (Floater f in floaters)
                T(f.Text, (float)f.X, (float)f.Y, f.Color, f6, StringAlignment.Center);
        }
        private void DrawHUD()
        {
            g.FillRectangle(hudBg, 0, 0, W, HudH);
            T("SCORE " + Pad((int)score), 4, 2, CWhite, f7, StringAlignment.Near);
            T("BEST " + Pad(best), W - 4, 2, CYellow, f7, StringAlignment.Far);
            for (int i = 0; i < Config.MaxLives; i++)
            {
                bool on = i < lives;
                R(5 + i * 7, 12, 5, 5, on ? CPlayer : CRoad2);
                if (on) R(6 + i * 7, 13, 3, 2, CWhite);
            }
            T("STG " + (stage + 1), 46, 11, CDash, f6, StringAlignment.Near);
            if (combo > 1)
                T("COMBO x" + combo, W - 4, 11, comboT < 0.8 ? CDash : CYellow, f6, StringAlignment.Far);
        }
        private void Panel(int y, int h)
        {
            g.FillRectangle(overlay, 12, y, 136, h);
            R(12, y, 136, 1, CWhite);
            R(12, y + h - 1, 136, 1, CWhite);
        }
        private void DrawTitle()
        {
            Panel(52, 134);
            T("PIXEL RACER", 80, 60, CYellow, f13, StringAlignment.Center);
            R(30, 78, 100, 1, CRoad2);
            T("DODGE THE TRAFFIC", 80, 84, CWhite, f7, StringAlignment.Center);
            T("NEAR MISS + GOLD COMBO", 80, 95, CWhite, f6, StringAlignment.Center);
            R(30, 107, 100, 1, CRoad2);
            T("ARROWS OR A / D", 80, 113, CDash, f7, StringAlignment.Center);
            T("3 LIVES   P PAUSE   M SOUND", 80, 124, CDash, f6, StringAlignment.Center);
            T("ANY KEY TO START", 80, 140, CYellow, f8, StringAlignment.Center);
            T("BEST " + Pad(best), 80, 156, CWhite, f7, StringAlignment.Center);
            T(muted ? "SOUND OFF (M)" : "SOUND ON (M)", 80, 169, CDash, f6, StringAlignment.Center);
        }
        private void DrawPause()
        {
            Panel(96, 44);
            T("PAUSED", 80, 104, CYellow, f13, StringAlignment.Center);
            T("PRESS P TO RESUME", 80, 124, CWhite, f7, StringAlignment.Center);
        }
        private void DrawOver()
        {
            Panel(70, 100);
            T("GAME OVER", 80, 78, CRed, f13, StringAlignment.Center);
            R(30, 97, 100, 1, CRoad2);
            T("SCORE " + Pad((int)score), 80, 104, CWhite, f8, StringAlignment.Center);
            T("BEST  " + Pad(best), 80, 117, CYellow, f8, StringAlignment.Center);
            T("REACHED STAGE " + (stage + 1), 80, 131, CDash, f7, StringAlignment.Center);
            T(newRecord ? "NEW RECORD!" : "ANY KEY TO RETRY",
              80, 150, newRecord ? CYellow : CWhite, f7, StringAlignment.Center);
        }

        // ---------- 波次系统与生成公平性 ----------
        private Enemy MakeEnemy(int x)
        {
            return new Enemy
            {
                X = x, Y = -CarH - 2,
                Color = Config.EnemyColors[RndInt(Config.EnemyColors.Length)]
            };
        }
        private double WaveInterval()
        {
            return Math.Max(Config.SpawnMin, Config.SpawnBase - stage * Config.SpawnStep)
                   * (0.9 + Rnd() * 0.2);
        }
        // 若所有空闲车道在来车到达前都不可达，就撤掉离玩家最远的一辆车。
        private void EnsureReachable(List<int> block)
        {
            double tArrive = (PlayerY + CarH * 2) / (speed * Config.EnemySpeedFactor);
            double budget = tArrive * Config.ReachMargin + Config.WarnTime;
            int gap = Lanes[1] - Lanes[0];
            for (int i = 0; i < Lanes.Length; i++)
            {
                if (!block.Contains(i) &&
                    Math.Abs(i - laneIndex) * gap / (double)Config.LaneSpeed <= budget) return;
            }
            if (block.Count == 0) return;
            int far = block[0];
            foreach (int b in block)
                if (Math.Abs(b - laneIndex) > Math.Abs(far - laneIndex)) far = b;
            block.Remove(far);
        }
        private void QueueWave()
        {
            if (waveQueue.Count == 0)
            {
                string[] pool = Config.WavePools[Config.WavePools.Length - 1].Patterns;
                foreach (Pool p in Config.WavePools)
                    if (stage <= p.UpToStage) { pool = p.Patterns; break; }
                string name = pool[RndInt(pool.Length)];
                Pattern def = Config.Patterns[name];
                if (def.Sequence)
                {
                    foreach (WaveDef wd in def.Waves)
                        waveQueue.Enqueue(new WavePlan(new List<int>(wd.Block), wd.Coin));
                }
                else
                {
                    WaveDef pick = def.Waves[RndInt(def.Waves.Length)];
                    waveQueue.Enqueue(new WavePlan(new List<int>(pick.Block), pick.Coin));
                }
            }
            WavePlan w = waveQueue.Dequeue();
            EnsureReachable(w.Block);
            warn = new WarnInfo { T = Config.WarnTime, Block = w.Block, Coin = w.Coin };
        }
        private void EmitWave()
        {
            EnsureReachable(warn.Block);
            foreach (int li in warn.Block) enemies.Add(MakeEnemy(Lanes[li]));
            if (warn.Coin >= 0)
            {
                int cx = Lanes[warn.Coin] + 4;
                coins.Add(new Coin { X = cx, Y = -12 });
                coins.Add(new Coin { X = cx, Y = -32 });
                coins.Add(new Coin { X = cx, Y = -52 });
            }
            warn = null;
        }
        private void SpawnCoin()
        {
            List<int> free = new List<int>();
            for (int i = 0; i < Lanes.Length; i++)
            {
                if (warn != null && warn.Block.Contains(i)) continue;
                int lane = Lanes[i];
                bool blocked = false;
                foreach (Enemy e in enemies)
                    if (e.Y < 50 && Math.Abs(e.X - lane) < 4) { blocked = true; break; }
                if (!blocked) free.Add(lane);
            }
            if (free.Count > 0) coins.Add(new Coin { X = free[RndInt(free.Count)] + 4, Y = -12 });
        }
        private void Burst(double x, double y, string color, int n)
        {
            for (int i = 0; i < n; i++)
            {
                parts.Add(new Part
                {
                    X = x + Rnd() * CarW, Y = y + Rnd() * CarH,
                    Vx = (Rnd() - 0.5) * 100, Vy = (Rnd() - 0.5) * 100 - 30,
                    Life = 0.5 + Rnd() * 0.6,
                    Color = Rnd() < 0.5 ? color : CWhite,
                    S = 1 + RndInt(2)
                });
            }
        }
        private void AddFloater(string s, double x, double y, string color)
        {
            floaters.Add(new Floater { Text = s, X = x, Y = y, Color = color, Life = 0.9 });
        }

        // 返回 true 表示本局结束，调用方应立即中断本帧更新。
        private bool HitEnemy(Enemy e)
        {
            lives--;
            invuln = Config.InvulnTime; hitStop = Config.HitStop;
            flash = Config.FlashTime; shake = 1;
            Burst(playerX, PlayerY, CPlayer, 18);
            Burst(e.X, e.Y, CWhite, 10);
            AddFloater(lives > 0 ? "HIT! -1 LIFE" : "WRECKED", playerX, PlayerY - 14, CRed);
            if (!muted) Beep(140, 70);
            if (lives <= 0) { GameOver(); return true; }
            return false;
        }

        // ---------- 逻辑（1/120 秒固定步长） ----------
        private void Update(double dt)
        {
            time += dt;
            if (flash > 0) flash = Math.Max(0, flash - dt);
            if (hitStop > 0) { hitStop = Math.Max(0, hitStop - dt); return; }
            if (shake > 0) shake = Math.Max(0, shake - dt * 3);

            for (int i = parts.Count - 1; i >= 0; i--)
            {
                Part p = parts[i];
                p.Life -= dt;
                if (p.Life <= 0) { parts.RemoveAt(i); continue; }
                p.Vy += 180 * dt; p.X += p.Vx * dt; p.Y += p.Vy * dt;
            }
            for (int i = floaters.Count - 1; i >= 0; i--)
            {
                Floater f = floaters[i];
                f.Life -= dt; f.Y -= 14 * dt;
                if (f.Life <= 0) floaters.RemoveAt(i);
            }
            if (state == St.Title) { scroll += 22 * dt; return; }
            if (state != St.Play) return;

            runTime += dt;
            stage = (int)(runTime / Config.StageSeconds);
            score += Config.ScorePerSecond * dt;
            speed = Math.Min(Config.SpeedMax, Config.SpeedBase + stage * Config.SpeedStep);
            scroll += speed * dt;
            if (invuln > 0) invuln = Math.Max(0, invuln - dt);
            if (comboT > 0) { comboT -= dt; if (comboT <= 0) combo = 0; }

            // 引擎脉冲：非阻塞、频率与节奏随速度变化（桌面版对 WebAudio 引擎音的近似）。
            engineT -= dt;
            if (engineT <= 0)
            {
                engineT = Math.Max(0.16, 0.34 - speed / 2600.0);
                if (!muted) Beep((int)(48 + speed * 0.42), 22);
            }

            int dir = left ? -1 : (right ? 1 : 0);
            if (dir != 0)
            {
                steerT -= dt;
                if (steerT <= 0)
                {
                    laneIndex = Clamp(laneIndex + dir, 0, Lanes.Length - 1);
                    steerT = Config.LaneRepeat;
                }
            }
            else steerT = 0;
            double target = Lanes[laneIndex], delta = target - playerX;
            double move = Config.LaneSpeed * dt;
            if (Math.Abs(delta) <= move) { playerX = target; tilt *= 0.6; }
            else { playerX += (delta > 0 ? 1 : -1) * move; tilt = Clamp(delta * 0.022, -2.6, 2.6); }

            if (warn == null)
            {
                spawnT -= dt;
                if (spawnT <= 0)
                {
                    bool clear = true;
                    foreach (Enemy e in enemies) if (e.Y <= 40) { clear = false; break; }
                    if (clear) { QueueWave(); spawnT = WaveInterval(); }
                    else spawnT = 0.15;
                }
            }
            else
            {
                warn.T -= dt;
                if (warn.T <= 0) EmitWave();
            }
            coinT -= dt;
            if (coinT <= 0)
            {
                SpawnCoin();
                coinT = Math.Max(1.2, Config.CoinBase - stage * 0.12) + Rnd() * Config.CoinJitter;
            }

            for (int i = enemies.Count - 1; i >= 0; i--)
            {
                Enemy e = enemies[i];
                e.Y += speed * Config.EnemySpeedFactor * dt;
                if (!e.Passed && e.Y > PlayerY + CarH) { e.Passed = true; score += Config.DodgeScore; }
                if (e.Y > H + 4) { enemies.RemoveAt(i); continue; }
                if (Overlap(playerX + 2, PlayerY + 2, CarW - 4, CarH - 4,
                            e.X + 2, e.Y + 2, CarW - 4, CarH - 4))
                {
                    enemies.RemoveAt(i);
                    if (invuln > 0) { Burst(e.X, e.Y, CWhite, 6); continue; }
                    if (HitEnemy(e)) return;
                    continue;
                }
                double dist = Math.Abs(e.X - playerX);
                if (!e.Near &&
                    e.Y < PlayerY + CarH && e.Y + CarH > PlayerY &&
                    dist > CarW - 3 && dist < CarW + Config.NearMissGap &&
                    Math.Abs(playerX - Lanes[laneIndex]) > 2)
                {
                    e.Near = true;
                    combo = Math.Min(Config.ComboMax, combo + 1); comboT = Config.ComboWindow;
                    int gain = (int)(Config.NearMissScore * combo);
                    score += gain;
                    AddFloater("+" + gain + " NEAR", e.X, e.Y, CWhite);
                    if (!muted) Beep(1180, 25);
                }
            }
            for (int i = coins.Count - 1; i >= 0; i--)
            {
                Coin c = coins[i];
                c.Y += speed * dt;
                if (c.Y > H + 8) { coins.RemoveAt(i); continue; }
                if (Overlap(playerX, PlayerY, CarW, CarH, c.X - 2, c.Y, 12, 12))
                {
                    coins.RemoveAt(i);
                    combo = Math.Min(Config.ComboMax, combo + 1); comboT = Config.ComboWindow;
                    int gain = (int)(Config.CoinScore * combo);
                    score += gain;
                    AddFloater("+" + gain + (combo > 1 ? " x" + combo : ""), c.X, c.Y, CYellow);
                    Burst(c.X, c.Y, CYellow, 8);
                    if (!muted) Beep(760 + (int)combo * 90, 30);
                }
            }
        }

        private void Render()
        {
            g.ResetTransform();
            R(0, 0, W, H, CBlack);
            if (shake > 0)
                g.TranslateTransform((float)((Rnd() - 0.5) * 4 * shake), (float)((Rnd() - 0.5) * 4 * shake));
            DrawRoad();
            DrawWarn();
            foreach (Coin c in coins) DrawCoin(c.X, c.Y);
            foreach (Enemy e in enemies) DrawCar(e.X, e.Y, e.Color, true, 0);
            bool blink = invuln > 0 && (int)Math.Floor(time * 14) % 2 == 0;
            if (state != St.Over && !blink) DrawCar(playerX, PlayerY, CPlayer, false, tilt);
            foreach (Part p in parts) R((int)p.X, (int)p.Y, p.S, p.S, p.Color);
            DrawFloaters();
            g.ResetTransform();
            DrawHUD();

            if (flash > 0)
            {
                int a = (int)(flash / Config.FlashTime * 0.5 * 255);
                flashBrush.Color = Color.FromArgb(a, 255, 255, 255);
                g.FillRectangle(flashBrush, 0, 0, W, H);
            }
            if (state == St.Title) DrawTitle();
            else if (state == St.Pause) DrawPause();
            else if (state == St.Over) DrawOver();
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            Render();
            Graphics gr = e.Graphics;
            gr.InterpolationMode = InterpolationMode.NearestNeighbor;
            gr.PixelOffsetMode = PixelOffsetMode.Half;
            gr.SmoothingMode = SmoothingMode.None;
            gr.DrawImage(back, 0, 0, ClientSize.Width, ClientSize.Height);
            base.OnPaint(e);
        }

        // ---------- 输入 ----------
        protected override bool ProcessCmdKey(ref Message msg, Keys keyData)
        {
            Keys k = keyData & Keys.KeyCode;
            bool first = held.Add(k);
            bool atMenu = state == St.Title || state == St.Over;
            bool modifier = k == Keys.ShiftKey || k == Keys.ControlKey ||
                            k == Keys.Menu || k == Keys.Tab || k == Keys.CapsLock;

            if (k == Keys.Left || k == Keys.A)
            {
                left = true;
                if (first && atMenu) Start();
                return true;
            }
            if (k == Keys.Right || k == Keys.D)
            {
                right = true;
                if (first && atMenu) Start();
                return true;
            }
            if (k == Keys.M) { if (first) muted = !muted; return true; }
            if (k == Keys.P || k == Keys.Escape)
            {
                if (first)
                {
                    if (state == St.Play) state = St.Pause;
                    else if (state == St.Pause) state = St.Play;
                }
                return true;
            }
            if (first && atMenu && !modifier) { Start(); return true; }
            if (first && state == St.Pause &&
                (k == Keys.Space || k == Keys.Enter || k == Keys.R))
            {
                state = St.Play;
                return true;
            }
            return base.ProcessCmdKey(ref msg, keyData);
        }

        protected override void OnKeyUp(KeyEventArgs e)
        {
            held.Remove(e.KeyCode);
            if (e.KeyCode == Keys.Left || e.KeyCode == Keys.A) left = false;
            if (e.KeyCode == Keys.Right || e.KeyCode == Keys.D) right = false;
            base.OnKeyUp(e);
        }

        protected override void OnDeactivate(EventArgs e)
        {
            left = right = false;
            held.Clear();
            if (state == St.Play) state = St.Pause;
            base.OnDeactivate(e);
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                timer.Dispose();
                g.Dispose();
                back.Dispose();
                overlay.Dispose();
                hudBg.Dispose();
                flashBrush.Dispose();
                f6.Dispose(); f7.Dispose(); f8.Dispose(); f13.Dispose();
                foreach (TextGlyph t in textCache.Values) t.Bmp.Dispose();
                foreach (SolidBrush b in brushes.Values) b.Dispose();
            }
            base.Dispose(disposing);
        }
    }
}
