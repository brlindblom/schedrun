#
# Needed functions for schedrun
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#

function module () {
	eval `/usr/bin/modulecmd bash $*`
}

function parse_optarg () {
	if [[ "$1" =~ "^--.*=.*" ]]; then
		echo ${1##*=}
	else
		shift
		if ! $SCHEDRUN_HOME/run --test $1 &> /dev/null; then
			echo $1
		fi
	fi
}

function valid_optarg () {
	if ! [[ "$2" =~ "$3" ]]; then
		echo "The value specified for $1 is invalid! Exiting..."
		echo "Use -h or --help for help."
		exit 1
	fi
}

# Debugging and profiling information
function extJobInfoLoop () {
    debugDir=$(pwd)/extDebugInfo.$JOB_ID
    profileInfo=$debugDir/jobProfileInfo
    envInfo=$debugDir/jobEnvInfo
    dmesgInfo=$debugDir/dmesgInfo
    ibInfo=$debugDir/ibInfo
    pidName=$1

    mkdir $debugDir

    cat $PE_HOSTFILE >> $envInfo

    if [ -n "$PE_HOSTFILE" ]; then
        hostList=$(awk '{ print $1 }' $PE_HOSTFILE | sort | uniq)
        hostList=$(echo $hostList | sed 's/\ /,/g')
        echo "Hostlist: $hostList"
    fi

    if groups | tr " " "\n" | grep -q '^admin$'; then
        PDSH_SSH_ARGS_APPEND="-i ${HOME}/.ssh/nopw"
    fi

    echo "Debugging enabled to $debugDir"
    echo
    echo "Files written: "

    echo $envInfo
    echo $dmesgInfo
    echo $ibInfo
    echo $profileInfo

    # Get basics
    cat $0 >> $envInfo
    echo; echo >> $envInfo
    env >> $envInfo
    echo; echo >> $envInfo
    ulimit -a >> $envInfo
    echo; echo >> $envInfo

    if [ -n "$hostList" ]; then
        pdsh -w $hostList -f 1 mount >> $envInfo
        echo; echo >> $envInfo
        pdsh -w $hostList -f 1 /sbin/ifconfig >> $envInfo
        echo; echo >> $envInfo
        pdsh -w $hostList -f 1 netstat -rn >> $envInfo
        pdsh -w $hostList -f 1 dmesg >> $dmesgInfo
        pdsh -w $hostList -f 1 /usr/sbin/ibstat >> $ibInfo
    else
        mount >> $envInfo
        echo >> $envInfo
        echo >> $envInfo
        /sbin/ifconfig -a >> $envInfo
        echo >> $envInfo
        echo >> $envInfo
        netstat -rn >> $envInfo
        dmesg >> $dmesgInfo
        /usr/sbin/ibstat >> $ibInfo
    fi

    if [ -n "$PE_HOSTFILE" ]; then
        #cmd="pdsh -w $hostList sudo -n $SCHEDRUN_HOME/profile-helper $JOB_ID"
        cmd="pdsh -w $hostList sudo /opt/admin/scripts/get-top-of-jid $JOB_ID"
    else
        #cmd="sudo $SCHEDRUN_HOME/profile-helper $JOB_ID"
        cmd="sudo /opt/admin/scripts/get-top-of-jid $JOB_ID"
    fi

    if [ -n "$pidName" ]; then
        $cmd | grep $pidName >> $profileInfo
    else
        $cmd >> $profileInfo
    fi 
}

# collect debug and profile information and place in
# DEBUG_DATA_DIR for sysadmin review
function extJobInfoCleanup () {
   kill $(jobs -rl | awk '/extJobInfoLoop/ { print $2 }')
}

function usage (){
	cat << EOF
HPC Job Execution Program - schedrun v0.1

Use arrow keys to navigate, 'q' to quit.

  schedrun [OPTIONS] [[COMMAND] [ARGUMENTS]]

  where [[COMMAND] [ARGUMENTS]] signify the command you wish to execute.  If 
  you are using pipes or redirects, the command must be enclosed in quotes.  
  You do not specify these if you are also specifying an Application Profile 
  (described below).

  The following are command options to modify the behavior of 'run'.  By 
  default, 'schedrun' will execute your application interactively with no parallel 
  features enabled.  More often than not, this is not what you want.

  -p [parallel_configuration] | --parallel[=<parallel_configuration>]
      By itself, this option enables the default Parallel Configuration,
      "$DEFAULT_PAR".  This option is required to run parallel jobs. 
      You can select from the following Parallel Configurations:

$(for p in $PAR_LIST; do echo "      * $p"; done)

  -n <int> | --num-cpus=<int>
      Specify the number of CPUs required for your parallel job.  Required if 
      specifying -p.

  -c <int> | --num-cpus-per-node=<int>
      Specify the number of CPUs to use per node.  Useful for MPI/OpenMP 
      applications.

  -t <time_val> | --time=<time_val>
      Specify the time required to execute your job.  Default for BATCH jobs 
      is 10 minutes.  <time_val> takes the format <HH...>HH:MM:SS

  -m <memory> | --memreq=<memory>
      Specify the total amount of memory that this job will use.
      <memory> takes the form of e.g. 16G, 0.2G, 256M, etc.

  -a <array_spec> | --array=<array_spec>
      Specify that this job is an array job (which spawns multiple copies of 
      the same job).  Within each copy of the job, the variable SGE_TASK_ID is 
      defined as a value within the range defined by <array_spec>.   
      <array_spec> takes the form of n-m, where n is a positive integer
      denoting the first task and m is a positive integer denoting the last 
      task.

  -f <features> | --feature=<features>
      A comma-separated list of features that should be requested for this job.
      Consult system documentation for list of features and their meaning.

  -M <email_addr> | --mail=<email_addr>
      Send job status updates to the email address defined by <email_addr>

  -N <name> | --name=<name>
      Assign a name for this job.  If this option is not specified, one will be 
      generated.

  -e <env_vars> | --environment=<env_vars>
      A comma-separated list of environment variables and assigned values that 
      will be set during job execution.

  -h | --help     - Display this help page.

  -b | --batch    - This job should run in batch-mode (in the background).

  -i | --inter    - This job should run in interactive-mode (in the foreground).
                    NOTE: If no resources are available, job will not start.
                    This mode is the DEFAULT

  -l | --list     - List available application profiles.

  -d | --debug    - Enable extra output during job run to diagnose problems.

  -D | --devel    - Specify that this job should run in the development queue.
                    30 minute limit applies.

  -v | --verbose  - See what this script is doing.

 Example Usage:

  1. Run "Matlab" in an interactive session:
     
     [user@host ~]$ schedrun matlab

     and run Matlab with more available memory:

     [user@host ~]$ schedrun -m 2G matlab

  2. Run a parallel application with 16 processors that needs 10 hours to 
     complete.  Notify you when complete and submit to the queue:

     [user@host ~]$ schedrun -b -p -n 16 -t 10:00:00 -m myemail@mydomain.com myapp

  3. Run an application from a profile that includes parallel support. In
     this case, we choose ANSYS 12 defined by the module 'apps/ansys/12':
     We see here that the command argument can be a valid executable OR 
     an 'apps' module:

     [user@host ~]$ schedrun -n 4 ansys/12

  4. Run an application from a profile that requires environment options and 
     four processors per node:

     [user@host ~]$ schedrun -n 16 -c 4 -e INPUT_DB=mydb.gdb gromacs/4.0.5

  5. Run an application in parallel that is tuned for a specific processor
     feature:

     [user@host ~]$ schedrun -p -n 32 -f sse4e -t 4:00:00 myapp

 Please report any issues to out Github issue tracker at 
 http://github.com/brichsmith/schedrun.  If reporting an issue with a job, please 
 re-run with the -d flag and attach the job's output.

EOF
}

# Zenity GTK functions

function zdir(){
        zenity --title="Select a directory..." --file-selection --directory
}

function zfile(){
	zenity --title="Select a file..." --file-selection
}

function zprompt(){
        zenity --title="Start a session?" --question --text "This will start a session for \"$1\".  Continue?"
}

function zprocs(){
	var=""
	while ! [[ "$var" =~ "^[1-9]{1}[0-9]*$" ]]; do
        	var=$(zenity --title="Processors" --entry --entry-text="1" --text "Specify the number of processors")
		[ -z "$var" ] && exit
	done
	echo $var
}

function zmem(){
        if [ -n "$MEMBASE" ]; then
                MEM=$MEMBASE
        else
                MEM=$DEFMEMBASE
        fi
	var=""
	while ! [[ $var =~ [0-9]+(\.[0-9])*(M|G) ]]; do
        	var=$(zenity --title="Memory" --entry --entry-text="$MEM" --text "Specify the memory required (e.g. 2.3G or 512M)")
		[ -z "$var" ] && exit
	done
	echo $var
}

function zrt(){
	var=""
	while ! [[ $var =~ ^[0-9]+:[0-9]+:[0-9]+ ]]; do
        	var=$(zenity --title="Run-time" --entry --entry-text="00:30:00" --text "Specify the required runtime")
		[ -z "$var" ] && exit
	done
	echo $var
}

function zemail(){
	var=""
	while ! [[ $var =~ ^([a-zA-Z0-9_\-\.]+)@((\[[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.)|(([a-zA-Z0-9\-]+\.)+))([a-zA-Z]{2,4}|[0-9]{1,3})(\]?)$ ]]; do
        	var=$(zenity --"Notification E-Mail" --entry --entry-text="$USER@mail.usf.edu" --text "Specify your e-mail address for notifications")
		[ -z "$var" ] && exit
	done
	echo $var
}
