##
# ui.rb
# Created June 20, 2013
# By Ron Bowes
##

require 'trollop' # We use this to parse commands
require 'readline' # For i/o operations

require 'subscribable'
require 'ui_command'
require 'ui_session_command'
require 'ui_session_interactive'

class Ui
  include Subscribable

  def initialize()
    @options = {}
    @thread = Thread.current()

    # There's always a single UiCommand in existance
    @command = nil

    # This is a handle to the current UI the user is interacting with
    @ui = nil

    # The current local_id
    @current_local_id = 0

    # This is a list of all UIs that are available, indexed by local_id
    @uis_by_local_id = {}
    @uis_by_real_id = {}

    # A mapping of real ids to session ids
    @id_map = {}

    # Lets us have multiple 'attached' sessions
    @ui_history = []

    initialize_subscribables()
  end

  class UiWakeup < Exception
    # Nothing required
  end

  def get_by_local_id(id)
    return @uis_by_local_id[id]
  end

  def get_by_real_id(id)
    return @uis_by_real_id[id]
  end

  def set_option(name, value)
    # Remove whitespace
    name  = name.to_s
    value = value.to_s

    name   = name.gsub(/^ */, '').gsub(/ *$/, '')
    value = value.gsub(/^ */, '').gsub(/ *$/, '')

    if(value == "nil")
      @options.delete(name)

      puts("#{name} => [deleted]")
    else

      # Replace \n with actual newlines
      value = value.gsub(/\\n/, "\n")

      # Replace true/false with the proper values
      value = true if(value == "true")
      value = false if(value == "false")

      # Validate the log level
      if(name == "log_level" && Log.get_by_name(value).nil?)
        puts("ERROR: Legal values for log_level are: #{Log::LEVELS}")
        return
      end

      @options[name] = value

      #puts("#{name} => #{value}")
    end
  end

  def each_option()
    @options.each_pair do |k, v|
      yield(k, v)
    end
  end

  def get_option(name)
    return @options[name]
  end

  def error(msg, local_id = nil)
    # Try to use the provided id first
    if(!local_id.nil?)
      ui = @uis_by_local_id[local_id]
      if(!ui.nil?)
        ui.error(msg)
        return
      end
    end

    # As a fall-back, or if the local_id wasn't provided, output to the current or
    # the command window
    if(@ui.nil?)
      @command.error(msg)
    else
      @ui.error(msg)
    end
  end

  # Detach the current session and attach a new one
  def attach_session(ui = nil)
    if(ui.nil?)
      ui = @command
    end

    # If the ui isn't changing, don't
    if(ui == @ui)
      return
    end

    # Detach the old ui
    if(!@ui.nil?)
      @ui_history << @ui
      @ui.detach()
    end

    # Go to the new ui
    @ui = ui

    # Attach the new ui
    @ui.attach()

    wakeup()
  end

  def detach_session()
    ui = @ui_history.pop()

    if(ui.nil?)
      ui = @command
    end

    if(!@ui.nil?)
      @ui.detach()
    end

    @ui = ui
    @ui.attach()

    wakeup()
  end

  def go()
    # Ensure that USR1 does nothing, see the 'hacks' section in the file
    # comment
#    Signal.trap("USR1") do
#      # Do nothing
#    end

    # There's always a single UiCommand in existance
    if(@command.nil?)
      @command = UiCommand.new(self)
    end

    begin
      attach_session(@command)
    rescue UiWakeup
      # Ignore
    end

    loop do
      begin
        if(@ui.nil?)
          Log.ERROR("@ui ended up nil somehow!")
        end

        # If the ui is no longer active, switch to the @command window
        if(!@ui.active?)
          @ui.error("UI went away...")
          detach_session()
        end

        @ui.go()

      rescue UiWakeup
        # Ignore the exception, it's just to break us out of the @ui.go() function
      rescue Exception => e
        puts(e)
        raise(e)
      end
    end
  end

  def each_ui()
    @uis_by_local_id.each do |s|
      yield(s)
    end
  end

  #################
  # The rest of this are callbacks
  #################

  def session_established(real_id)
    # Generate the local id
    local_id = @current_local_id + 1
    @current_local_id += 1

    # Create the mapping
    @id_map[real_id] = local_id

    # Get a handle to the session
    session = SessionManager.find(real_id)

    # Fail if it doesn't exist
    if(session.nil?)
      raise(DnscatException, "Couldn't find the new session!")
    end

    # Create a new UI
    if(session.is_command)
      ui = UiSessionCommand.new(local_id, session, self)
      self.subscribe(ui)
    else
      ui = UiSessionInteractive.new(local_id, session, self)
      self.subscribe(ui)
    end

    # Save it in both important lists
    @uis_by_local_id[local_id] = ui
    @uis_by_real_id[real_id]   = ui

    # Let all the other sessions know that this one was created
    notify_subscribers(:ui_created, [ui, local_id, real_id])

    # If nobody else has claimed it, bequeath it to the root (command) ui
    if(ui.parent.nil?)
      ui.parent = @command

      # Since the @command window has no way to know that it's supposed to have
      # this session, add it manually
      @command.ui_created(ui, local_id, real_id, true)
    else
      ui.parent.output("New session established: #{local_id}")
    end

    @command.output("New session established: #{local_id}")
  end

  def session_data_received(real_id, data)
    ui = @uis_by_real_id[real_id]
    if(ui.nil?)
      raise(DnscatException, "Couldn't find session: #{real_id}")
    end
    ui.feed(data)
  end

  def session_data_acknowledged(real_id, data)
    ui = @uis_by_real_id[real_id]
    if(ui.nil?)
      raise(DnscatException, "Couldn't find session: #{real_id}")
    end
    ui.ack(data)
  end

  def session_destroyed(real_id)
    ui = @uis_by_real_id[real_id]
    if(ui.nil?)
      raise(DnscatException, "Couldn't find session: #{real_id}")
    end

    # Tell the UI it's been destroyed
    ui.destroy()

    # Switch the session for @command if it's attached
    if(@ui == ui)
      detach_session()
    end
  end

  # This is used by the 'kill' command the user can enter
  def kill_session(local_id)
    # Find the session
    ui = @uis_by_local_id[local_id]
    if(ui.nil?())
      return false
    end

    # Kill it
    ui.session.kill()

    return true
  end

  # Callback
  def session_heartbeat(real_id)
    ui = @uis_by_real_id[real_id]
    if(ui.nil?)
      raise(DnscatException, "Couldn't find session: #{real_id}")
    end
    ui.heartbeat()
  end

  # Callback
  def dnscat2_session_error(real_id, message)
    ui = @uis_by_real_id[real_id]
    if(ui.nil?)
      raise(DnscatException, "Couldn't find session: #{real_id}")
    end
    ui.error(message)
  end

  # Callback
  def log(level, message)
    # Handle the special case, before a level is set
    if(@options["log_level"].nil?)
      min = Log::INFO
    else
      min = Log.get_by_name(@options["log_level"])
    end

    if(level >= min)
      # TODO: @command is occasionally nil here - consider creating it earlier?
      if(@command.nil?)
        puts("[[#{Log::LEVELS[level]}]] :: #{message}")
      else
        @command.error("[[#{Log::LEVELS[level]}]] :: #{message}")
      end
    end
  end

  def wakeup()
    @thread.raise(UiWakeup)
  end
end

