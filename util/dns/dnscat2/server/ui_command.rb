# ui_command.rb
# By Ron Bowes
# Created July 4, 2013

require 'readline'

require 'parser'
require 'ui_handler'
require 'ui_interface'

class UiCommand < UiInterface
  include Parser
  include UiHandler

  def register_commands()
    register_alias('q',    'quit')
    register_alias('exit', 'quit')

    register_command('echo',
        Trollop::Parser.new do
          banner("Print stuff to the terminal")
        end,

        Proc.new do |opts, optval|
          puts(optval)
        end,
    )

    register_command('quit',
        Trollop::Parser.new do
          banner("Exits dnscat2")
        end,

        Proc.new do |opts, optval|
          exit
        end
    )

    register_command('help',
        Trollop::Parser.new do
          banner("Shows a help menu")
        end,

        Proc.new do |opts, optval|
          puts("Here are the available commands, listed alphabetically:")
          @commands.keys.sort.each do |name|
            # Don't display the empty command
            if(name != "")
              puts("- #{name}")
            end
          end

          puts("For more information, --help can be passed to any command")
        end,
    )

    register_command('clear',
      Trollop::Parser.new do
      end,

      Proc.new do |opts, optval|
        0.upto(1000) do puts() end
      end,
    )

    register_command('sessions',
      Trollop::Parser.new do
        banner("Lists the current active sessions")
        opt :all, "Show dead sessions", :type => :boolean, :required => false
      end,

      Proc.new do |opts, optval|
        display_uis(opts[:all])
      end,
    )

    register_command("session",
      Trollop::Parser.new do
        banner("Interact with a session")
        opt :i, "Interact with the chosen session", :type => :integer, :required => false
      end,

      Proc.new do |opts, optval|
        if(opts[:i].nil?)
          puts("Known sessions:")
          display_uis(false)
        else
          ui = @ui.get_by_local_id(opts[:i])
          if(ui.nil?)
            error("Session #{opts[:i]} not found!")
            display_uis(false)
          else
            @ui.attach_session(ui)
          end
        end
      end
    )

    register_command("set",
      Trollop::Parser.new do
        banner("Set <name>=<value> variables")
      end,

      Proc.new do |opts, optarg|
        if(optarg.length == 0)
          puts("Usage: set <name>=<value>")
          puts()
          do_show_options()
        else
          optarg = optarg.join(" ")

          # Split at the '=' sign
          optarg = optarg.split("=", 2)

          # If we don't have a name=value setup, show an error
          if(optarg.length != 2)
            puts("Usage: set <name>=<value>")
          else
            @ui.set_option(optarg[0], optarg[1])
          end
        end
      end
    )

    register_command("show",
      Trollop::Parser.new do
        banner("Shows current variables if 'show options' is run. Currently no other functionality")
      end,

      Proc.new do |opts, optarg|
        if(optarg != "options")
          puts("Usage: show options")
        else
          do_show_options()
        end
      end
    )

    register_command("kill",
      Trollop::Parser.new do
        banner("Terminate a session")
      end,

      Proc.new do |opts, optarg|
        if(optarg.nil? || optarg.to_i == 0)
          puts("Usage: kill <session_id>")
        else
          if(@ui.kill_session(optarg[0].to_i()))
            puts("Session killed")
          else
            puts("Couldn't kill session!")
          end
        end
      end
    )
  end

  def do_show_options()
    @ui.each_option do |name, value|
      puts("#{name} => #{value}")
    end
  end

  def initialize(ui)
    super()

    initialize_ui_handler()
    initialize_parser("dnscat2> ")
    @ui = ui
    register_commands()
  end

  def active?()
    return true
  end

  def to_s()
    return "command window"
  end

  def destroy()
    raise(DnscatException, "Tried to kill the command window!")
  end

  def output(str)
    puts()
    puts(str)

    if(attached?())
      print(">> ")
    end
  end

  def error(str)
    puts("%s" % str)
  end
end
