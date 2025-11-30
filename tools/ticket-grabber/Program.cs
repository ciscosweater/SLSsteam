using SteamKit2;
using SteamKit2.Internal;
using SteamKit2.Authentication;

namespace TicketGrabber
{
    public sealed class EncryptedAppTicketCallback : CallbackMsg
    {
        //Stolen from SteamKit2/Steam/Handlers/SteamApps/Callbacks.cs:AppOwnershipTicketCallback
        public EResult Result { get; private set; }
        public uint AppID { get; private set; }
        public SteamKit2.Internal.EncryptedAppTicket Ticket { get; private set; }

        internal EncryptedAppTicketCallback(IPacketMsg packetMsg)
        {
            var ticketResponse = new ClientMsgProtobuf<CMsgClientRequestEncryptedAppTicketResponse>(packetMsg);
            var msg = ticketResponse.Body;

            this.JobID = ticketResponse.TargetJobID;

            this.Result = (EResult)msg.eresult;
            this.AppID = msg.app_id;
            this.Ticket = msg.encrypted_app_ticket;
        }
    }

    public sealed partial class EncryptedTicketCallbackHandler : ClientMsgHandler
    {
        public override void HandleMsg(IPacketMsg msg)
        {
            CallbackMsg callback = null;

            switch (msg.MsgType)
            {
                case EMsg.ClientRequestEncryptedAppTicketResponse:
                    callback = new EncryptedAppTicketCallback(msg);
                    break;
            }

            if (callback != null)
            {
                this.Client.PostCallback(callback);
            }
        }
    }

    class TicketGrabber
    {
        public readonly string GuardDataDir = Path.Combine(Environment.CurrentDirectory, "Sentries");
        public readonly string TicketDir = Path.Combine(Environment.CurrentDirectory, "Tickets");

        protected bool finished = false;
        protected CallbackManager callbackManager;

        protected string guardDataFile => Path.Combine(GuardDataDir, Username + ".sentry");

        protected string getStoredGuardData()
        {
            if (!Directory.Exists(GuardDataDir))
            {
                Directory.CreateDirectory(GuardDataDir);
            }

            if (!File.Exists(guardDataFile))
            {
                return null;
            }

            return File.ReadAllText(guardDataFile);
        }

        protected void storeGuardData(string guardData)
        {
            if (!File.Exists(guardDataFile))
            {
                File.Create(guardDataFile).Close();
            }

            File.WriteAllText(guardDataFile, guardData);
        }

        protected void storeTicket(string prefix, List<byte> ticket, uint maxLen)
        {
            if (!Directory.Exists(TicketDir))
            {
                Directory.CreateDirectory(TicketDir);
            }

            //Shitty code to convert. Can't be arsed to improve this right now
            while (ticket.Count < maxLen)
            {
                ticket.Add(0);
            }

            var filePath = Path.Combine(TicketDir, $"{prefix}{TargetAppId}");
            File.WriteAllBytes(filePath, ticket.ToArray());
            Console.WriteLine($"Saved {filePath}");
        }

        public readonly string Username;
        public readonly string Password;
        public readonly uint TargetAppId;

        public SteamClient Client;
        public SteamUser User;
        public SteamFriends Friends;
        public SteamApps Apps;

        public TicketGrabber(string username, string password, uint targetAppId)
        {
            Username = username;
            Password = password;
            TargetAppId = targetAppId;

            Client = new SteamClient();
            callbackManager = new CallbackManager(Client);

            User = Client.GetHandler<SteamUser>();
            Friends = Client.GetHandler<SteamFriends>();
            Apps = Client.GetHandler<SteamApps>();

            if
            (
                User == null
                || Friends == null
                || Apps == null
            )
            {
                Console.WriteLine("Failed to get Handlers!");
                Environment.Exit(1);
            }

            Client.AddHandler(new EncryptedTicketCallbackHandler());

            callbackManager.Subscribe<SteamClient.ConnectedCallback>(OnConnected);
            callbackManager.Subscribe<SteamClient.DisconnectedCallback>(OnDisconnected);

            callbackManager.Subscribe<SteamUser.LoggedOnCallback>(OnLoggedOn);
            callbackManager.Subscribe<SteamUser.LoggedOffCallback>(OnLoggedOff);
            callbackManager.Subscribe<SteamUser.AccountInfoCallback>(OnAccountInfo);
        }

        async protected void OnConnected(SteamClient.ConnectedCallback cb)
        {
            Console.WriteLine("Connected to Steam! Logging in...");

            var details = new SteamKit2.Authentication.AuthSessionDetails
            {
                Username = Username,
                Password = Password,
                IsPersistentSession = true,
                Authenticator = new UserConsoleAuthenticator()
            };

            var guardData = getStoredGuardData();
            if (guardData != null)
            {
                //TODO: Add to details directly?
                details.GuardData = guardData;
            }

            var authSess = await Client.Authentication.BeginAuthSessionViaCredentialsAsync(details);
            var resp = await authSess.PollingWaitForResultAsync();

            if (resp.NewGuardData != null)
            {
                storeGuardData(resp.NewGuardData);
            }

            User.LogOn(new SteamUser.LogOnDetails
            {
                Username = resp.AccountName,
                AccessToken = resp.RefreshToken,
                ShouldRememberPassword = true
            });
        }

        protected void OnDisconnected(SteamClient.DisconnectedCallback cb)
        {
            Console.WriteLine("Disconnected from Steam! Exiting...");
        }

        protected void OnLoggedOn(SteamUser.LoggedOnCallback cb)
        {
            Console.WriteLine($"Logged in as {Username}");
        }

        protected void OnLoggedOff(SteamUser.LoggedOffCallback cb)
        {
            Console.WriteLine($"Logged out from {Username}");
        }

        protected void OnAccountInfo(SteamUser.AccountInfoCallback cb)
        {
            Console.WriteLine("Account Info received! Going online...");

            Friends.SetPersonaState(EPersonaState.Online);
            RequestTickets(TargetAppId);
        }

        public void SendGamesPlayed(ulong appId)
        {
            var msg = new ClientMsgProtobuf<CMsgClientGamesPlayed>(EMsg.ClientGamesPlayed);
            msg.Body.games_played.Add(new CMsgClientGamesPlayed.GamePlayed
            {
                game_id = appId
            });

            Client.Send(msg);
        }

        public AsyncJob<EncryptedAppTicketCallback> RequestEncryptedAppTicket(uint appId)
        {
            var msg = new ClientMsgProtobuf<CMsgClientRequestEncryptedAppTicket>(EMsg.ClientRequestEncryptedAppTicket);
            msg.SourceJobID = Client.GetNextJobID();

            msg.Body.app_id = appId;
            msg.Body.userdata = [];

            Client.Send(msg);

            return new AsyncJob<EncryptedAppTicketCallback>(Client, msg.SourceJobID);
        }

        async public void RequestTickets(uint appId)
        {
            SendGamesPlayed(appId);

            var ticket = await Apps.GetAppOwnershipTicket((uint)appId);
            if (ticket.Result != EResult.OK)
            {
                Console.WriteLine($"Failed GetAppOwnershipTicket! ({ticket.Result})");
                Environment.Exit(1);
            }
            Console.WriteLine("AppOwnershipTicket received!");

            var encryptedTicket = await RequestEncryptedAppTicket(appId);
            if (ticket.Result != EResult.OK)
            {
                Console.WriteLine($"Failed RequestEncryptedAppTicket! ({ticket.Result})");
                Environment.Exit(1);
            }
            Console.WriteLine("EncryptedAppTicket received!");

            //Current ticket sizes in SLSsteam
            storeTicket("ticket_", ticket.Ticket.ToList(), 0x400);

            var slsTicket = encryptedTicket.Ticket.encrypted_ticket.ToList();
            slsTicket.InsertRange(0, BitConverter.GetBytes(slsTicket.Count));
            slsTicket.InsertRange(0, BitConverter.GetBytes(User.SteamID.AccountID));
            storeTicket("encryptedTicket_", slsTicket, 0x1008); //Add two extra uints for size + AccountID

            finished = true;
        }

        public void CallbackLoop()
        {
            while (!finished)
            {
                callbackManager.RunWaitCallbacks(TimeSpan.FromSeconds(1));
            }
        }

        public void Connect()
        {
            Client.Connect();
        }
    }

    static class Program
    {
        public static void Main(string[] args)
        {
            if (args.Length < 3 || (args.Length > 0 && args.Any(a => a == "-h" || a == "--help")))
            {
                Console.WriteLine("Usage: ./ticket-grabber username password appId");
                Environment.Exit(1);
            }

            uint appId;
            if (!uint.TryParse(args[2], out appId))
            {
                Console.WriteLine($"{args[2]} is not a number!");
                Environment.Exit(1);
            }

            var grabber = new TicketGrabber(args[0], args[1], appId);
            grabber.Connect();
            grabber.CallbackLoop();
        }
    }
}
