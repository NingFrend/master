# SPDX-License-Identifier: GPL-2.0-or-later
#
# (c) 2010-2018 Joe Perches <joe@perches.com>
use Encode qw(decode encode);
my $frr = 1;

my $verbose = 0;
my %verbose_messages = ();
my %verbose_emitted = ();
my $gitroot = $ENV{'GIT_DIR'};
$gitroot = ".git" if !defined($gitroot);
my $max_line_length = 100;
my $user_codespellfile = "";
my $conststructsfile = "$D/const_structs.checkpatch";
my $docsfile = "$D/../doc/developer/checkpatch.rst";
my $typedefsfile;
my $allow_c99_comments = 0; # Not in FRR.
#my $allow_c99_comments = 1; # Can be overridden by --ignore C99_COMMENT_TOLERANCE
# git output parsing needs US English output, so first set backtick child process LANGUAGE
my $git_command ='export LANGUAGE=en_US.UTF-8; git';
my $tabsize = 8;
my ${CONFIG_} = "CONFIG_";
  -v, --verbose              verbose mode
  --max-line-length=n        set the maximum line length, (default $max_line_length)
                             if exceeded, warn on patches
                             requires --strict for use with --file
  --tab-size=n               set the number of spaces for tab (default $tabsize)
                             (default:$codespellfile)
  --kconfig-prefix=WORD      use WORD as a prefix for Kconfig symbols (default
                             ${CONFIG_})
	my %types = ();
	while ($text =~ /(?:(\bCHK|\bWARN|\bERROR|&\{\$msg_level})\s*\(|\$msg_type\s*=)\s*"([^"]+)"/g) {
		if (defined($1)) {
			if (exists($types{$2})) {
				$types{$2} .= ",$1" if ($types{$2} ne $1);
			} else {
				$types{$2} = $1;
			}
		} else {
			$types{$2} = "UNDETERMINED";
		}

	if ($color) {
		print(" ( Color coding: ");
		print(RED . "ERROR" . RESET);
		print(" | ");
		print(YELLOW . "WARNING" . RESET);
		print(" | ");
		print(GREEN . "CHECK" . RESET);
		print(" | ");
		print("Multiple levels / Undetermined");
		print(" )\n\n");
	}

	foreach my $type (sort keys %types) {
		my $orig_type = $type;
		if ($color) {
			my $level = $types{$type};
			if ($level eq "ERROR") {
				$type = RED . $type . RESET;
			} elsif ($level eq "WARN") {
				$type = YELLOW . $type . RESET;
			} elsif ($level eq "CHK") {
				$type = GREEN . $type . RESET;
			}
		}
		if ($verbose && exists($verbose_messages{$orig_type})) {
			my $message = $verbose_messages{$orig_type};
			$message =~ s/\n/\n\t/g;
			print("\t" . $message . "\n\n");
		}
sub load_docs {
	open(my $docs, '<', "$docsfile")
	    or warn "$P: Can't read the documentation file $docsfile $!\n";

	my $type = '';
	my $desc = '';
	my $in_desc = 0;

	while (<$docs>) {
		chomp;
		my $line = $_;
		$line =~ s/\s+$//;

		if ($line =~ /^\s*\*\*(.+)\*\*$/) {
			if ($desc ne '') {
				$verbose_messages{$type} = trim($desc);
			}
			$type = $1;
			$desc = '';
			$in_desc = 1;
		} elsif ($in_desc) {
			if ($line =~ /^(?:\s{4,}|$)/) {
				$line =~ s/^\s{4}//;
				$desc .= $line;
				$desc .= "\n";
			} else {
				$verbose_messages{$type} = trim($desc);
				$type = '';
				$desc = '';
				$in_desc = 0;
			}
		}
	}

	if ($desc ne '') {
		$verbose_messages{$type} = trim($desc);
	}
	close($docs);
}

	'v|verbose!'	=> \$verbose,
	'tab-size=i'	=> \$tabsize,
	'codespellfile=s'	=> \$user_codespellfile,
	'kconfig-prefix=s'	=> \${CONFIG_},
) or $help = 2;

if ($user_codespellfile) {
	# Use the user provided codespell file unconditionally
	$codespellfile = $user_codespellfile;
} elsif (!(-f $codespellfile)) {
	# If /usr/share/codespell/dictionary.txt is not present, try to find it
	# under codespell's install directory: <codespell_root>/data/dictionary.txt
	if (($codespell || $help) && which("codespell") ne "" && which("python") ne "") {
		my $python_codespell_dict = << "EOF";

import os.path as op
import codespell_lib
codespell_dir = op.dirname(codespell_lib.__file__)
codespell_file = op.join(codespell_dir, 'data', 'dictionary.txt')
print(codespell_file, end='')
EOF

		my $codespell_dict = `python -c "$python_codespell_dict" 2> /dev/null`;
		$codespellfile = $codespell_dict if (-f $codespell_dict);
	}
}
# $help is 1 if either -h, --help or --version is passed as option - exitcode: 0
# $help is 2 if invalid option is passed - exitcode: 1
help($help - 1) if ($help);

die "$P: --git cannot be used with --file or --fix\n" if ($git && ($file || $fix));
die "$P: --verbose cannot be used with --terse\n" if ($verbose && $terse);

if ($color =~ /^[01]$/) {
	$color = !$color;
} elsif ($color =~ /^always$/i) {
	$color = 1;
} elsif ($color =~ /^never$/i) {
	$color = 0;
} elsif ($color =~ /^auto$/i) {
	$color = (-t STDOUT);
} else {
	die "$P: Invalid color mode: $color\n";
}
load_docs() if ($verbose);
my $perl_version_ok = 1;
	$perl_version_ok = 0;
	exit(1) if (!$ignore_perl_version);
# skip TAB size 1 to avoid additional checks on $tabsize - 1
die "$P: Invalid TAB size: $tabsize\n" if ($tabsize < 2);
			__refconst|
			__refdata|
			const|
			volatile|
			__ro_after_init|
			__weak|
			__alloc_size\s*\(\s*\d+\s*(?:,\s*\d+\s*)?\)
our $String	= qr{(?:\b[Lu])?"[X\t]*"};
			darr_foreach_p|darr_foreach_i|frr_each|frr_each_safe|frr_each_from|
			FOR_ALL_INTERFACES|JSON_FOREACH|
                        FOREACH_BE_CLIENT_BITS|FOREACH_MGMTD_BE_CLIENT_ID|
our $allocFunctions = qr{(?x:
	(?:(?:devm_)?
		(?:kv|k|v)[czm]alloc(?:_array)?(?:_node)? |
		kstrdup(?:_const)? |
		kmemdup(?:_nul)?) |
	(?:\w+)?alloc_skb(?:_ip_align)? |
				# dev_alloc_skb/netdev_alloc_skb, et al
	dma_alloc_coherent
)};

	Co-developed-by:|
our $tracing_logging_tags = qr{(?xi:
	[=-]*> |
	<[=-]* |
	\[ |
	\] |
	start |
	called |
	entered |
	entry |
	enter |
	in |
	inside |
	here |
	begin |
	exit |
	end |
	done |
	leave |
	completed |
	out |
	return |
	[\.\!:\s]*
)};

sub edit_distance_min {
	my (@arr) = @_;
	my $len = scalar @arr;
	if ((scalar @arr) < 1) {
		# if underflow, return
		return;
	}
	my $min = $arr[0];
	for my $i (0 .. ($len-1)) {
		if ($arr[$i] < $min) {
			$min = $arr[$i];
		}
	}
	return $min;
}

sub get_edit_distance {
	my ($str1, $str2) = @_;
	$str1 = lc($str1);
	$str2 = lc($str2);
	$str1 =~ s/-//g;
	$str2 =~ s/-//g;
	my $len1 = length($str1);
	my $len2 = length($str2);
	# two dimensional array storing minimum edit distance
	my @distance;
	for my $i (0 .. $len1) {
		for my $j (0 .. $len2) {
			if ($i == 0) {
				$distance[$i][$j] = $j;
			} elsif ($j == 0) {
				$distance[$i][$j] = $i;
			} elsif (substr($str1, $i-1, 1) eq substr($str2, $j-1, 1)) {
				$distance[$i][$j] = $distance[$i - 1][$j - 1];
			} else {
				my $dist1 = $distance[$i][$j - 1]; #insert distance
				my $dist2 = $distance[$i - 1][$j]; # remove
				my $dist3 = $distance[$i - 1][$j - 1]; #replace
				$distance[$i][$j] = 1 + edit_distance_min($dist1, $dist2, $dist3);
			}
		}
	}
	return $distance[$len1][$len2];
}

sub find_standard_signature {
	my ($sign_off) = @_;
	my @standard_signature_tags = (
		'Signed-off-by:', 'Co-developed-by:', 'Acked-by:', 'Tested-by:',
		'Reviewed-by:', 'Reported-by:', 'Suggested-by:'
	);
	foreach my $signature (@standard_signature_tags) {
		return $signature if (get_edit_distance($sign_off, $signature) <= 2);
	}

	return "";
}

my $word_pattern = '\b[A-Z]?[a-z]{2,}\b';

$mode_perms_search = "(?:${mode_perms_search})";

our %deprecated_apis = (
	"synchronize_rcu_bh"			=> "synchronize_rcu",
	"synchronize_rcu_bh_expedited"		=> "synchronize_rcu_expedited",
	"call_rcu_bh"				=> "call_rcu",
	"rcu_barrier_bh"			=> "rcu_barrier",
	"synchronize_sched"			=> "synchronize_rcu",
	"synchronize_sched_expedited"		=> "synchronize_rcu_expedited",
	"call_rcu_sched"			=> "call_rcu",
	"rcu_barrier_sched"			=> "rcu_barrier",
	"get_state_synchronize_sched"		=> "get_state_synchronize_rcu",
	"cond_synchronize_sched"		=> "cond_synchronize_rcu",
);

#Create a search pattern for all these strings to speed up a loop below
our $deprecated_apis_search = "";
foreach my $entry (keys %deprecated_apis) {
	$deprecated_apis_search .= '|' if ($deprecated_apis_search ne "");
	$deprecated_apis_search .= $entry;
}
$deprecated_apis_search = "(?:${deprecated_apis_search})";
our $single_mode_perms_string_search = "(?:${mode_perms_string_search})";
our $multi_mode_perms_string_search = qr{
	${single_mode_perms_string_search}
	(?:\s*\|\s*${single_mode_perms_string_search})*
}x;

sub perms_to_octal {
	my ($string) = @_;

	return trim($string) if ($string =~ /^\s*0[0-7]{3,3}\s*$/);

	my $val = "";
	my $oval = "";
	my $to = 0;
	my $curpos = 0;
	my $lastpos = 0;
	while ($string =~ /\b(($single_mode_perms_string_search)\b(?:\s*\|\s*)?\s*)/g) {
		$curpos = pos($string);
		my $match = $2;
		my $omatch = $1;
		last if ($lastpos > 0 && ($curpos - length($omatch) != $lastpos));
		$lastpos = $curpos;
		$to |= $mode_permission_string_types{$match};
		$val .= '\s*\|\s*' if ($val ne "");
		$val .= $match;
		$oval .= $omatch;
	}
	$oval =~ s/^\s*\|\s*//;
	$oval =~ s/\s*\|\s*$//;
	return sprintf("%04o", $to);
}
} elsif (!$frr) {
			$$wordsRef .= '|' if (defined $$wordsRef);
my $const_structs;
if (!$frr &&
    show_type("CONST_STRUCT")) {
	read_words(\$const_structs, $conststructsfile)
	    or warn "No structs that should be const will be found - file '$conststructsfile': $!\n";
}

if (defined($typedefsfile)) {
	my $typeOtherTypedefs;
	$typeTypedefs .= '|' . $typeOtherTypedefs if (defined $typeOtherTypedefs);
			(?:(?:\s|\*|\[\])+\s*const|(?:\s|\*\s*(?:const\s*)?|\[\])+|(?:\s*\[\s*\])+){0,4}
			(?:(?:\s|\*|\[\])+\s*const|(?:\s|\*\s*(?:const\s*)?|\[\])+|(?:\s*\[\s*\])+){0,4}
	(?:SKCIPHER_REQUEST|SHASH_DESC|AHASH_REQUEST)_ON_STACK\s*\(
our %allow_repeated_words = (
	add => '',
	added => '',
	bad => '',
	be => '',
);

our %maintained_status = ();

	if (!exists($maintained_status{$filename})) {
		$maintained_status{$filename} = `perl $root/scripts/get_maintainer.pl --status --nom --nol --nogit --nogit-fallback -f $filename 2>&1`;
	}

	return $maintained_status{$filename} =~ /obsolete/i;
}

sub is_SPDX_License_valid {
	my ($license) = @_;
	return 1 if (!$tree || which("python3") eq "" || !(-x "$root/scripts/spdxcheck.py") || !(-e "$gitroot"));

	my $root_path = abs_path($root);
	my $status = `cd "$root_path"; echo "$license" | scripts/spdxcheck.py -`;
	return 0 if ($status ne "");
	return 1;
	if (-e "$gitroot") {
		my $git_last_include_commit = `${git_command} log --no-merges --pretty=format:"%h%n" -1 -- include`;
	if (-e "$gitroot") {
		$files = `${git_command} ls-files "include/*.h"`;
sub git_is_single_file {
	my ($filename) = @_;

	return 0 if ((which("git") eq "") || !(-e "$gitroot"));

	my $output = `${git_command} ls-files -- $filename 2>/dev/null`;
	my $count = $output =~ tr/\n//;
	return $count eq 1 && $output =~ m{^${filename}$};
}

	return ($id, $desc) if ((which("git") eq "") || !(-e "$gitroot"));
	my $output = `${git_command} log --no-color --format='%H %s' -1 $commit 2>&1`;
	if ($lines[0] =~ /^error: short SHA1 $commit is ambiguous/) {
	} elsif ($lines[0] =~ /^fatal: ambiguous argument '$commit': unknown revision or path not in the working tree\./ ||
		 $lines[0] =~ /^fatal: bad object $commit/) {
die "$P: No git repository found\n" if ($git && !-e "$gitroot");
		my $lines = `${git_command} log --no-color --no-merges --pretty=format:'%H %s' $git_range`;
$allow_c99_comments = !defined $ignore_type{"C99_COMMENT_TOLERANCE"};
	my $is_git_file = git_is_single_file($filename);
	my $oldfile = $file;
	$file = 1 if ($is_git_file);
		$vname = qq("$1") if ($filename eq '-' && $_ =~ m/^Subject:\s+(.+)/i);
	$file = $oldfile if ($is_git_file);
	if (!$perl_version_ok) {
      An upgrade to at least perl $minimum_perl_version is suggested.

	my $quoted = "";
	my $name_comment = "";
		$formatted_email =~ s/\Q$address\E.*$//;
	# Extract comments from names excluding quoted parts
	# "John D. (Doe)" - Do not extract
	if ($name =~ s/\"(.+)\"//) {
		$quoted = $1;
	}
	while ($name =~ s/\s*($balanced_parens)\s*/ /) {
		$name_comment .= trim($1);
	}
	$name =~ s/^[ \"]+|[ \"]+$//g;
	$name = trim("$quoted $name");

	$comment = trim($comment);
	return ($name, $name_comment, $address, $comment);
	my ($name, $name_comment, $address, $comment) = @_;
	$name =~ s/^[ \"]+|[ \"]+$//g;
	$address =~ s/(?:\.|\,|\")+$//; ##trailing commas, dots or quotes
	$name_comment = trim($name_comment);
	$name_comment = " $name_comment" if ($name_comment ne "");
	$comment = trim($comment);
	$comment = " $comment" if ($comment ne "");

		$formatted_email = "$name$name_comment <$address>";
	$formatted_email .= "$comment";
sub reformat_email {
	my ($email) = @_;

	my ($email_name, $name_comment, $email_address, $comment) = parse_email($email);
	return format_email($email_name, $name_comment, $email_address, $comment);
}

sub same_email_addresses {
	my ($email1, $email2) = @_;

	my ($email1_name, $name1_comment, $email1_address, $comment1) = parse_email($email1);
	my ($email2_name, $name2_comment, $email2_address, $comment2) = parse_email($email2);

	return $email1_name eq $email2_name &&
	       $email1_address eq $email2_address &&
	       $name1_comment eq $name2_comment &&
	       $comment1 eq $comment2;
}

			for (; ($n % $tabsize) != 0; $n++) {
		# Comments we are whacking completely including the begin
	if ($allow_c99_comments && $res =~ m@(//.*$)@) {
		my $match = $1;
		$res =~ s/\Q$match\E/"$;" x length($match)/e;
	}

	return "" if (!defined($line) || !defined($rawline));
	# If c99 comment on the current line, or the line before or after
	my ($current_comment) = ($rawlines[$end_line - 1] =~ m@^\+.*(//.*$)@);
	return $current_comment if (defined $current_comment);
	($current_comment) = ($rawlines[$end_line - 2] =~ m@^[\+ ].*(//.*$)@);
	return $current_comment if (defined $current_comment);
	($current_comment) = ($rawlines[$end_line] =~ m@^[\+ ].*(//.*$)@);
	return $current_comment if (defined $current_comment);

	($current_comment) = ($rawlines[$end_line - 1] =~ m@.*(/\*.*\*/)\s*(?:\\\s*)?$@);
sub get_stat_real {
	my ($linenr, $lc) = @_;

	my $stat_real = raw_line($linenr, 0);
	for (my $count = $linenr + 1; $count <= $lc; $count++) {
		$stat_real = $stat_real . "\n" . raw_line($count, 0);
	}

	return $stat_real;
}

sub get_stat_here {
	my ($linenr, $cnt, $here) = @_;

	my $herectx = $here . "\n";
	for (my $n = 0; $n < $cnt; $n++) {
		$herectx .= raw_line($linenr, $n) . "\n";
	}

	return $herectx;
}


	if ($terse) {
		$output = (split('\n', $output))[0] . "\n";
	}

	if ($verbose && exists($verbose_messages{$type}) &&
	    !exists($verbose_emitted{$type})) {
		$output .= $verbose_messages{$type} . "\n\n";
		$verbose_emitted{$type} = 1;
	}
	my $source_indent = $tabsize;
sub get_raw_comment {
	my ($line, $rawline) = @_;
	my $comment = '';

	for my $i (0 .. (length($line) - 1)) {
		if (substr($line, $i, 1) eq "$;") {
			$comment .= substr($rawline, $i, 1);
		}

	return $comment;
}

sub exclude_global_initialisers {
	my ($realfile) = @_;

	# Do not check for BPF programs (tools/testing/selftests/bpf/progs/*.c, samples/bpf/*_kern.c, *.bpf.c).
	return $realfile =~ m@^tools/testing/selftests/bpf/progs/.*\.c$@ ||
		$realfile =~ m@^samples/bpf/.*_kern\.c$@ ||
		$realfile =~ m@/bpf/.*\.bpf\.c$@;
}

sub remove_defuns {
    my @breakfast = ();
    my $milktoast;
    for my $tasty (@rawlines) {
        $milktoast = $tasty;
        if (($tasty =~ /^\+DEFPY/ ||
             $tasty =~ /^\+DEFUN/ ||
             $tasty =~ /^\+ALIAS/) .. ($tasty =~ /^\+\{/)) {
            $milktoast = "\n";
        }
        push(@breakfast, $milktoast);
    }
    @rawlines = @breakfast;
	my $author = '';
	my $authorsignoff = 0;
	my $author_sob = '';
	my $is_binding_patch = -1;
	my $has_patch_separator = 0;	#Found a --- line
	my $commit_log_lines = 0;	#Number of commit log lines
	my $last_git_commit_id_linenr = -1;

	my $checklicenseline = 1;

        remove_defuns();
			if ($1 =~ m@Documentation/admin-guide/kernel-parameters.txt$@) {
		my $raw_comment = get_raw_comment($line, $rawline);

# check if it's a mode change, rename or start of a patch
		if (!$in_commit_log &&
		    ($line =~ /^ mode change [0-7]+ => [0-7]+ \S+\s*$/ ||
		    ($line =~ /^rename (?:from|to) \S+\s*$/ ||
		     $line =~ /^diff --git a\/[\w\/\.\_\-]+ b\/\S+\s*$/))) {
			$is_patch = 1;
		}
			$checklicenseline = 1;

			if ($realfile !~ /^MAINTAINERS/) {
				my $last_binding_patch = $is_binding_patch;

				$is_binding_patch = () = $realfile =~ m@^(?:Documentation/devicetree/|include/dt-bindings/)@;

				if (($last_binding_patch != -1) &&
				    ($last_binding_patch ^ $is_binding_patch)) {
					WARN("DT_SPLIT_BINDING_PATCH",
					     "DT binding docs and includes should be a separate patch. See: Documentation/devicetree/bindings/submitting-patches.rst\n");
				}
			}

# Verify the existence of a commit log if appropriate
# 2 is used because a $signature is counted in $commit_log_lines
		if ($in_commit_log) {
			if ($line !~ /^\s*$/) {
				$commit_log_lines++;	#could be a $signature
			}
		} elsif ($has_commit_log && $commit_log_lines < 2) {
			# FRR: not used by FRR
			# WARN("COMMIT_MESSAGE",
			#      "Missing commit description - Add an appropriate one\n");
			$commit_log_lines = 2;	#warn only once
		}

		    (($line =~ m@^\s+diff\b.*a/([\w/]+)@ &&
		      $line =~ m@^\s+diff\b.*a/[\w/]+\s+b/$1\b@) ||
# Check the patch for a From:
		if (decode("MIME-Header", $line) =~ /^From:\s*(.*)/) {
			$author = $1;
			my $curline = $linenr;
			while(defined($rawlines[$curline]) && ($rawlines[$curline++] =~ /^[ \t]\s*(.*)/)) {
				$author .= $1;
			}
			$author = encode("utf8", $author) if ($line =~ /=\?utf-8\?/i);
			$author =~ s/"//g;
			$author = reformat_email($author);
		}

		if ($line =~ /^\s*signed-off-by:\s*(.*)/i) {
			if ($author ne ''  && $authorsignoff != 1) {
				if (same_email_addresses($1, $author)) {
					$authorsignoff = 1;
				} else {
					my $ctx = $1;
					my ($email_name, $email_comment, $email_address, $comment1) = parse_email($ctx);
					my ($author_name, $author_comment, $author_address, $comment2) = parse_email($author);

					if (lc $email_address eq lc $author_address && $email_name eq $author_name) {
						$author_sob = $ctx;
						$authorsignoff = 2;
					} elsif (lc $email_address eq lc $author_address) {
						$author_sob = $ctx;
						$authorsignoff = 3;
					} elsif ($email_name eq $author_name) {
						$author_sob = $ctx;
						$authorsignoff = 4;

						my $address1 = $email_address;
						my $address2 = $author_address;

						if ($address1 =~ /(\S+)\+\S+(\@.*)/) {
							$address1 = "$1$2";
						}
						if ($address2 =~ /(\S+)\+\S+(\@.*)/) {
							$address2 = "$1$2";
						}
						if ($address1 eq $address2) {
							$authorsignoff = 5;
						}
					}
				}
			}
		}

# Check for patch separator
		if ($line =~ /^---$/) {
			$has_patch_separator = 1;
			$in_commit_log = 0;
				my $suggested_signature = find_standard_signature($sign_off);
				if ($suggested_signature eq "") {
					WARN("BAD_SIGN_OFF",
					     "Non-standard signature: $sign_off\n" . $herecurr);
				} else {
					if (WARN("BAD_SIGN_OFF",
						 "Non-standard signature: '$sign_off' - perhaps '$suggested_signature'?\n" . $herecurr) &&
					    $fix) {
						$fixed[$fixlinenr] =~ s/$sign_off/$suggested_signature/;
					}
				}
			my ($email_name, $name_comment, $email_address, $comment) = parse_email($email);
			my $suggested_email = format_email(($email_name, $name_comment, $email_address, $comment));
				if (!same_email_addresses($email, $suggested_email)) {
					if (WARN("BAD_SIGN_OFF",
						 "email address '$email' might be better as '$suggested_email'\n" . $herecurr) &&
					    $fix) {
						$fixed[$fixlinenr] =~ s/\Q$email\E/$suggested_email/;
					}
				}

				# Address part shouldn't have comments
				my $stripped_address = $email_address;
				$stripped_address =~ s/\([^\(\)]*\)//g;
				if ($email_address ne $stripped_address) {
					if (WARN("BAD_SIGN_OFF",
						 "address part of email should not have comments: '$email_address'\n" . $herecurr) &&
					    $fix) {
						$fixed[$fixlinenr] =~ s/\Q$email_address\E/$stripped_address/;
					}
				}

				# Only one name comment should be allowed
				my $comment_count = () = $name_comment =~ /\([^\)]+\)/g;
				if ($comment_count > 1) {
					     "Use a single name comment in email: '$email'\n" . $herecurr);
				}


				# stable@vger.kernel.org or stable@kernel.org shouldn't
				# have an email name. In addition comments should strictly
				# begin with a #
				if ($email =~ /^.*stable\@(?:vger\.)?kernel\.org/i) {
					if (($comment ne "" && $comment !~ /^#.+/) ||
					    ($email_name ne "")) {
						my $cur_name = $email_name;
						my $new_comment = $comment;
						$cur_name =~ s/[a-zA-Z\s\-\"]+//g;

						# Remove brackets enclosing comment text
						# and # from start of comments to get comment text
						$new_comment =~ s/^\((.*)\)$/$1/;
						$new_comment =~ s/^\[(.*)\]$/$1/;
						$new_comment =~ s/^[\s\#]+|\s+$//g;

						$new_comment = trim("$new_comment $cur_name") if ($cur_name ne $new_comment);
						$new_comment = " # $new_comment" if ($new_comment ne "");
						my $new_email = "$email_address$new_comment";

						if (WARN("BAD_STABLE_ADDRESS_STYLE",
							 "Invalid email format for stable: '$email', prefer '$new_email'\n" . $herecurr) &&
						    $fix) {
							$fixed[$fixlinenr] =~ s/\Q$email\E/$new_email/;
						}
					}
				} elsif ($comment ne "" && $comment !~ /^(?:#.+|\(.+\))$/) {
					my $new_comment = $comment;

					# Extract comment text from within brackets or
					# c89 style /*...*/ comments
					$new_comment =~ s/^\[(.*)\]$/$1/;
					$new_comment =~ s/^\/\*(.*)\*\/$/$1/;

					$new_comment = trim($new_comment);
					$new_comment =~ s/^[^\w]$//; # Single lettered comment with non word character is usually a typo
					$new_comment = "($new_comment)" if ($new_comment ne "");
					my $new_email = format_email($email_name, $name_comment, $email_address, $new_comment);

					if (WARN("BAD_SIGN_OFF",
						 "Unexpected content after email: '$email', should be: '$new_email'\n" . $herecurr) &&
					    $fix) {
						$fixed[$fixlinenr] =~ s/\Q$email\E/$new_email/;
					}

# Check Co-developed-by: immediately followed by Signed-off-by: with same name and email
			if ($sign_off =~ /^co-developed-by:$/i) {
				if ($email eq $author) {
					WARN("BAD_SIGN_OFF",
					      "Co-developed-by: should not be used to attribute nominal patch author '$author'\n" . "$here\n" . $rawline);
				}
				if (!defined $lines[$linenr]) {
					WARN("BAD_SIGN_OFF",
					     "Co-developed-by: must be immediately followed by Signed-off-by:\n" . "$here\n" . $rawline);
				} elsif ($rawlines[$linenr] !~ /^\s*signed-off-by:\s*(.*)/i) {
					WARN("BAD_SIGN_OFF",
					     "Co-developed-by: must be immediately followed by Signed-off-by:\n" . "$here\n" . $rawline . "\n" .$rawlines[$linenr]);
				} elsif ($1 ne $email) {
					WARN("BAD_SIGN_OFF",
					     "Co-developed-by and Signed-off-by: name/email do not match \n" . "$here\n" . $rawline . "\n" .$rawlines[$linenr]);
				}
			}
# Check for Gerrit Change-Ids not in any patch context
		if ($realfile eq '' && !$has_patch_separator && $line =~ /^\s*change-id:/i) {
			if (ERROR("GERRIT_CHANGE_ID",
			          "Remove Gerrit Change-Id's before submitting upstream\n" . $herecurr) &&
			    $fix) {
				fix_delete_line($fixlinenr, $rawline);
			}
		     $line =~ /^\s*\[\<[0-9a-fA-F]{8,}\>\]/) ||
		     $line =~ /^(?:\s+\w+:\s+[0-9a-fA-F]+){3,3}/ ||
		     $line =~ /^\s*\#\d+\s*\[[0-9a-fA-F]+\]\s*\w+ at [0-9a-fA-F]+/) {
					# stack dump address styles
		      $line =~ /^\s*(?:Fixes:|Link:|$signature_tags)/i ||
					# A Fixes: or Link: line or signature tag line
# Check for lines starting with a #
		if ($in_commit_log && $line =~ /^#/) {
			if (WARN("COMMIT_COMMENT_SYMBOL",
				 "Commit log lines starting with '#' are dropped by git as comments\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~ s/^/ /;
			}
		}

# A correctly formed commit description is:
#    commit <SHA-1 hash length 12+ chars> ("Complete commit subject")
# with the commit subject '("' prefix and '")' suffix
# This is a fairly compilicated block as it tests for what appears to be
# bare SHA-1 hash with  minimum length of 5.  It also avoids several types of
# possible SHA-1 matches.
# A commit match can span multiple lines so this block attempts to find a
# complete typical commit on a maximum of 3 lines
		if ($perl_version_ok &&
		    $in_commit_log && !$commit_log_possible_stack_dump &&
		    $line !~ /^\s*(?:Link|Patchwork|http|https|BugLink|base-commit):/i &&
		    (($line =~ /\bcommit\s+[0-9a-f]{5,}\b/i ||
		      ($line =~ /\bcommit\s*$/i && defined($rawlines[$linenr]) && $rawlines[$linenr] =~ /^\s*[0-9a-f]{5,}\b/i)) ||
			my $herectx = $herecurr;
			my $has_parens = 0;
			my $has_quotes = 0;

			my $input = $line;
			if ($line =~ /(?:\bcommit\s+[0-9a-f]{5,}|\bcommit\s*$)/i) {
				for (my $n = 0; $n < 2; $n++) {
					if ($input =~ /\bcommit\s+[0-9a-f]{5,}\s*($balanced_parens)/i) {
						$orig_desc = $1;
						$has_parens = 1;
						# Always strip leading/trailing parens then double quotes if existing
						$orig_desc = substr($orig_desc, 1, -1);
						if ($orig_desc =~ /^".*"$/) {
							$orig_desc = substr($orig_desc, 1, -1);
							$has_quotes = 1;
						}
						last;
					}
					last if ($#lines < $linenr + $n);
					$input .= " " . trim($rawlines[$linenr + $n]);
					$herectx .= "$rawlines[$linenr + $n]\n";
				}
				$herectx = $herecurr if (!$has_parens);
			}
			if ($input =~ /\b(c)ommit\s+([0-9a-f]{5,})\b/i) {
				$short = 0 if ($input =~ /\bcommit\s+[0-9a-f]{12,40}/i);
				$long = 1 if ($input =~ /\bcommit\s+[0-9a-f]{41,}/i);
				$space = 0 if ($input =~ /\bcommit [0-9a-f]/i);
				$case = 0 if ($input =~ /\b[Cc]ommit\s+[0-9a-f]{5,40}[^A-F]/);
			} elsif ($input =~ /\b([0-9a-f]{12,40})\b/i) {
			    ($short || $long || $space || $case || ($orig_desc ne $description) || !$has_quotes) &&
			    $last_git_commit_id_linenr != $linenr - 1) {
				      "Please use git commit description style 'commit <12+ chars of sha1> (\"<title line>\")' - ie: '${init_char}ommit $id (\"$description\")'\n" . $herectx);
			#don't report the next line if this line ends in commit and the sha1 hash is the next line
			$last_git_commit_id_linenr = $linenr if ($line =~ /\bcommit\s*$/i);
			# WARN("FILE_PATH_CHANGES",
			#      "added, moved or deleted file(s), does MAINTAINERS need updating?\n" . $herecurr);
		}

# Check for adding new DT bindings not in schema format
		if (!$in_commit_log &&
		    ($line =~ /^new file mode\s*\d+\s*$/) &&
		    ($realfile =~ m@^Documentation/devicetree/bindings/.*\.txt$@)) {
			WARN("DT_SCHEMA_BINDING_PATCH",
			     "DT bindings should be in DT schema format. See: Documentation/devicetree/bindings/writing-schema.rst\n");
			while ($rawline =~ /(?:^|[^\w\-'`])($misspellings)(?:[^\w\-'`]|$)/gi) {
				my $blank = copy_spacing($rawline);
				my $ptr = substr($blank, 0, $-[1]) . "^" x length($typo);
				my $hereptr = "$hereline$ptr\n";
						  "'$typo' may be misspelled - perhaps '$typo_fix'?\n" . $hereptr) &&
# check for invalid commit id
		if ($in_commit_log && $line =~ /(^fixes:|\bcommit)\s+([0-9a-f]{6,40})\b/i) {
			my $id;
			my $description;
			($id, $description) = git_commit_info($2, undef, undef);
			if (!defined($id)) {
				WARN("UNKNOWN_COMMIT_ID",
				     "Unknown commit id '$2', maybe rebased or not pulled?\n" . $herecurr);
			}
		}

# check for repeated words separated by a single space
# avoid false positive from list command eg, '-rw-r--r-- 1 root root'
		if (($rawline =~ /^\+/ || $in_commit_log) &&
		    $rawline !~ /[bcCdDlMnpPs\?-][rwxsStT-]{9}/) {
			pos($rawline) = 1 if (!$in_commit_log);
			while ($rawline =~ /\b($word_pattern) (?=($word_pattern))/g) {

				my $first = $1;
				my $second = $2;
				my $start_pos = $-[1];
				my $end_pos = $+[2];
				if ($first =~ /(?:struct|union|enum)/) {
					pos($rawline) += length($first) + length($second) + 1;
					next;
				}

				next if (lc($first) ne lc($second));
				next if ($first eq 'long');

				# check for character before and after the word matches
				my $start_char = '';
				my $end_char = '';
				$start_char = substr($rawline, $start_pos - 1, 1) if ($start_pos > ($in_commit_log ? 0 : 1));
				$end_char = substr($rawline, $end_pos, 1) if ($end_pos < length($rawline));

				next if ($start_char =~ /^\S$/);
				next if (index(" \t.,;?!", $end_char) == -1);

				# avoid repeating hex occurrences like 'ff ff fe 09 ...'
				if ($first =~ /\b[0-9a-f]{2,}\b/i) {
					next if (!exists($allow_repeated_words{lc($first)}));
				}

				if (WARN("REPEATED_WORD",
					 "Possible repeated word: '$first'\n" . $herecurr) &&
				    $fix) {
					$fixed[$fixlinenr] =~ s/\b$first $second\b/$first/;
				}
			}

			# if it's a repeated word on consecutive lines in a comment block
			if ($prevline =~ /$;+\s*$/ &&
			    $prevrawline =~ /($word_pattern)\s*$/) {
				my $last_word = $1;
				if ($rawline =~ /^\+\s*\*\s*$last_word /) {
					if (WARN("REPEATED_WORD",
						 "Possible repeated word: '$last_word'\n" . $hereprev) &&
					    $fix) {
						$fixed[$fixlinenr] =~ s/(\+\s*\*\s*)$last_word /$1/;
					}
				}
			}
		}

# Check for FSF mailing addresses.
		if ($rawline =~ /\bwrite to the Free/i ||
		    $rawline =~ /\b675\s+Mass\s+Ave/i ||
		    $rawline =~ /\b59\s+Temple\s+Pl/i ||
		    $rawline =~ /\b51\s+Franklin\s+St/i) {
			my $herevet = "$here\n" . cat_vet($rawline) . "\n";
			my $msg_level = \&ERROR;
			$msg_level = \&CHK if ($file);
			&{$msg_level}("FSF_MAILING_ADDRESS",
				      "Do not include the paragraph about writing to the Free Software Foundation's mailing address from the sample GPL notice. The FSF has changed addresses in the past, and may do so again. Linux already includes a copy of the GPL.\n" . $herevet)
		}

		    # 'choice' is usually the last thing on the line (though
		    # Kconfig supports named choices), so use a word boundary
		    # (\b) rather than a whitespace character (\s)
		    $line =~ /^\+\s*(?:config|menuconfig|choice)\b/) {
				if ($lines[$ln - 1] =~ /^\+\s*(?:bool|tristate|prompt)\s*["']/) {

				# This only checks context lines in the patch
				# and so hopefully shouldn't trigger false
				# positives, even though some of these are
				# common words in help texts
				if ($f =~ /^\s*(?:config|menuconfig|choice|endchoice|
						  if|endif|menu|endmenu|source)\b/x) {
# check MAINTAINERS entries
		if ($realfile =~ /^MAINTAINERS$/) {
# check MAINTAINERS entries for the right form
			if ($rawline =~ /^\+[A-Z]:/ &&
			    $rawline !~ /^\+[A-Z]:\t\S/) {
				if (WARN("MAINTAINERS_STYLE",
					 "MAINTAINERS entries use one tab after TYPE:\n" . $herecurr) &&
				    $fix) {
					$fixed[$fixlinenr] =~ s/^(\+[A-Z]):\s*/$1:\t/;
				}
			}
# check MAINTAINERS entries for the right ordering too
			my $preferred_order = 'MRLSWQBCPTFXNK';
			if ($rawline =~ /^\+[A-Z]:/ &&
			    $prevrawline =~ /^[\+ ][A-Z]:/) {
				$rawline =~ /^\+([A-Z]):\s*(.*)/;
				my $cur = $1;
				my $curval = $2;
				$prevrawline =~ /^[\+ ]([A-Z]):\s*(.*)/;
				my $prev = $1;
				my $prevval = $2;
				my $curindex = index($preferred_order, $cur);
				my $previndex = index($preferred_order, $prev);
				if ($curindex < 0) {
					WARN("MAINTAINERS_STYLE",
					     "Unknown MAINTAINERS entry type: '$cur'\n" . $herecurr);
				} else {
					if ($previndex >= 0 && $curindex < $previndex) {
						WARN("MAINTAINERS_STYLE",
						     "Misordered MAINTAINERS entry - list '$cur:' before '$prev:'\n" . $hereprev);
					} elsif ((($prev eq 'F' && $cur eq 'F') ||
						  ($prev eq 'X' && $cur eq 'X')) &&
						 ($prevval cmp $curval) > 0) {
						WARN("MAINTAINERS_STYLE",
						     "Misordered MAINTAINERS entry - list file patterns in alphabetic order\n" . $hereprev);
					}
				}
			my $vp_file = $dt_path . "vendor-prefixes.yaml";
				`grep -Eq "\\"\\^\Q$vendor\E,\\.\\*\\":" $vp_file`;
# check for using SPDX license tag at beginning of files
		if ($realline == $checklicenseline) {
			if ($rawline =~ /^[ \+]\s*\#\!\s*\//) {
				$checklicenseline = 2;
			} elsif ($rawline =~ /^\+/) {
				my $comment = "";
				if (!$frr &&
                                    $realfile =~ /\.(h|s|S)$/) {
					$comment = '/*';
				} elsif ($frr &&
                                         $realfile =~ /\.(h|s|S)$/) {
                                    $comment = '//';
				} elsif ($realfile =~ /\.(c|dts|dtsi)$/) {
					$comment = '//';
				} elsif (($checklicenseline == 2) || $realfile =~ /\.(sh|pl|py|awk|tc|yaml)$/) {
					$comment = '#';
				} elsif ($realfile =~ /\.rst$/) {
					$comment = '..';
				}

# check SPDX comment style for .[chsS] files
				if ($realfile =~ /\.[chsS]$/ &&
				    $rawline =~ /SPDX-License-Identifier:/ &&
				    $rawline !~ m@^\+\s*\Q$comment\E\s*@) {
					WARN("SPDX_LICENSE_TAG",
					     "Improper SPDX comment style for '$realfile', please use '$comment' instead\n" . $herecurr);
				}

				if ($comment !~ /^$/ &&
				    $rawline !~ m@^\+\Q$comment\E SPDX-License-Identifier: @) {
					WARN("SPDX_LICENSE_TAG",
					     "Missing or malformed SPDX-License-Identifier tag in line $checklicenseline\n" . $herecurr);
				} elsif ($rawline =~ /(SPDX-License-Identifier: .*)/) {
					my $spdx_license = $1;
					if (!is_SPDX_License_valid($spdx_license)) {
						WARN("SPDX_LICENSE_TAG",
						     "'$spdx_license' is not supported in LICENSES/...\n" . $herecurr);
					}
					if ($realfile =~ m@^Documentation/devicetree/bindings/@ &&
					    not $spdx_license =~ /GPL-2\.0.*BSD-2-Clause/) {
						my $msg_level = \&WARN;
						$msg_level = \&CHK if ($file);
						if (&{$msg_level}("SPDX_LICENSE_TAG",

								  "DT binding documents should be licensed (GPL-2.0-only OR BSD-2-Clause)\n" . $herecurr) &&
						    $fix) {
							$fixed[$fixlinenr] =~ s/SPDX-License-Identifier: .*/SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause)/;
						}
					}
				}
			}
		}

# check for embedded filenames
		if ($rawline =~ /^\+.*\Q$realfile\E/) {
			WARN("EMBEDDED_FILENAME",
			     "It's generally not useful to have the filename in the file\n" . $herecurr);
		}

# check for using SPDX-License-Identifier on the wrong line number
		if ($realline != $checklicenseline &&
		    $rawline =~ /\bSPDX-License-Identifier:/ &&
		    substr($line, @-, @+ - @-) eq "$;" x (@+ - @-)) {
			WARN("SPDX_LICENSE_TAG",
			     "Misplaced SPDX-License-Identifier tag - use line $checklicenseline instead\n" . $herecurr);
		}

#	lines with an RFC3986 like URL
			# URL ($rawline is used in case the URL is in a comment)
			} elsif ($rawline =~ /^\+.*\b[a-z][\w\.\+\-]*:\/\/\S+/i) {
				$msg_type = "";

				my $msg_level = \&WARN;
				$msg_level = \&CHK if ($file);
				&{$msg_level}($msg_type,
					      "line length of $length exceeds $max_line_length columns\n" . $herecurr);
			if (WARN("MISSING_EOF_NEWLINE",
			         "adding a line without newline at end of file\n" . $herecurr) &&
			    $fix) {
				fix_delete_line($fixlinenr+1, "No newline at end of file");
			}
# check for .L prefix local symbols in .S files
		if ($realfile =~ /\.S$/ &&
		    $line =~ /^\+\s*(?:[A-Z]+_)?SYM_[A-Z]+_(?:START|END)(?:_[A-Z_]+)?\s*\(\s*\.L/) {
			WARN("AVOID_L_PREFIX",
			     "Avoid using '.L' prefixed local symbol names for denoting a range of code via 'SYM_*_START/END' annotations; see Documentation/asm-annotations.rst\n" . $herecurr);
# more than $tabsize must use tabs.
					   s/(^\+.*) {$tabsize,$tabsize}\t/$1\t\t/) {}
# check for assignments on the start of a line
		if ($sline =~ /^\+\s+($Assignment)[^=]/) {
			my $operator = $1;
			if (CHK("ASSIGNMENT_CONTINUATIONS",
				"Assignment operator '$1' should be on the previous line\n" . $hereprev) &&
			    $fix && $prevrawline =~ /^\+/) {
				# add assignment operator to the previous line, remove from current line
				$fixed[$fixlinenr - 1] .= " $operator";
				$fixed[$fixlinenr] =~ s/\Q$operator\E\s*//;
			}
		}

			my $operator = $1;
			if (CHK("LOGICAL_CONTINUATIONS",
				"Logical continuations should be on the previous line\n" . $hereprev) &&
			    $fix && $prevrawline =~ /^\+/) {
				# insert logical operator at last non-comment, non-whitepsace char on previous line
				$prevline =~ /[\s$;]*$/;
				my $line_end = substr($prevrawline, $-[0]);
				$fixed[$fixlinenr - 1] =~ s/\Q$line_end\E$/ $operator$line_end/;
				$fixed[$fixlinenr] =~ s/\Q$operator\E\s*//;
			}
		if ($perl_version_ok &&
		    $sline =~ /^\+\t+( +)(?:$c90_Keywords\b|\{\s*$|\}\s*(?:else\b|while\b|\s*$)|$Declare\s*$Ident\s*[;=])/) {
			if ($indent % $tabsize) {
					$fixed[$fixlinenr] =~ s@(^\+\t+) +@$1 . "\t" x ($indent/$tabsize)@e;
		if ($perl_version_ok &&
					"\t" x ($pos / $tabsize) .
					" "  x ($pos % $tabsize);
		    $realline > 3) { # Do not warn about the initial copyright comment block after SPDX-License-Identifier
# (declarations must have the same indentation and not be at the start of line)
		if (($prevline =~ /\+(\s+)\S/) && $sline =~ /^\+$1\S/) {
			# use temporaries
			my $sl = $sline;
			my $pl = $prevline;
			# remove $Attribute/$Sparse uses to simplify comparisons
			$sl =~ s/\b(?:$Attribute|$Sparse)\b//g;
			$pl =~ s/\b(?:$Attribute|$Sparse)\b//g;
			if (($pl =~ /^\+\s+$Declare\s*$Ident\s*[=,;:\[]/ ||
			     $pl =~ /^\+\s+$Declare\s*\(\s*\*\s*$Ident\s*\)\s*[=,;:\[\(]/ ||
			     $pl =~ /^\+\s+$Ident(?:\s+|\s*\*\s*)$Ident\s*[=,;\[]/ ||
			     $pl =~ /^\+\s+$declaration_macros/) &&
			    !($pl =~ /^\+\s+$c90_Keywords\b/ ||
			      $pl =~ /(?:$Compare|$Assignment|$Operators)\s*$/ ||
			      $pl =~ /(?:\{\s*|\\)$/) &&
			    !($sl =~ /^\+\s+$Declare\s*$Ident\s*[=,;:\[]/ ||
			      $sl =~ /^\+\s+$Declare\s*\(\s*\*\s*$Ident\s*\)\s*[=,;:\[\(]/ ||
			      $sl =~ /^\+\s+$Ident(?:\s+|\s*\*\s*)$Ident\s*[=,;\[]/ ||
			      $sl =~ /^\+\s+$declaration_macros/ ||
			      $sl =~ /^\+\s+(?:static\s+)?(?:const\s+)?(?:union|struct|enum|typedef)\b/ ||
			      $sl =~ /^\+\s+(?:$|[\{\}\.\#\"\?\:\(\[])/ ||
			      $sl =~ /^\+\s+$Ident\s*:\s*\d+\s*[,;]/ ||
			      $sl =~ /^\+\s+\(?\s*(?:$Compare|$Assignment|$Operators)/)) {
				if (WARN("LINE_SPACING",
					 "Missing a blank line after declarations\n" . $hereprev) &&
				    $fix) {
					fix_insert_line($fixlinenr, "\+");
				}
# if the previous line is a goto, return or break
# and is indented the same # of tabs
			if ($prevline =~ /^\+$tabs(goto|return|break)\b/) {
				if (WARN("UNNECESSARY_BREAK",
					 "break is not useful after a $1\n" . $hereprev) &&
				    $fix) {
					fix_delete_line($fixlinenr, $rawline);
				}
			    (($sindent % $tabsize) != 0 ||
			     ($sindent > $indent + $tabsize))) {
# check for self assignments used to avoid compiler warnings
# e.g.:	int foo = foo, *bar = NULL;
#	struct foo bar = *(&(bar));
		if ($line =~ /^\+\s*(?:$Declare)?([A-Za-z_][A-Za-z\d_]*)\s*=/) {
			my $var = $1;
			if ($line =~ /^\+\s*(?:$Declare)?$var\s*=\s*(?:$var|\*\s*\(?\s*&\s*\(?\s*$var\s*\)?\s*\)?)\s*[;,]/) {
				WARN("SELF_ASSIGNMENT",
				     "Do not use self-assignments to avoid compiler warnings\n" . $herecurr);
			}
		}

			if (!$allow_c99_comments && ERROR("C99_COMMENTS",
				  "do not use C99 // comments\n" . $herecurr) &&
			    $fix) {
				my $line = $fixed[$fixlinenr];
				if ($line =~ /\/\/(.*)$/) {
					my $comment = trim($1);
					$fixed[$fixlinenr] =~ s@\/\/(.*)$@/\* $comment \*/@;
		    ($lines[$realline_next - 1] =~ /EXPORT_SYMBOL.*\((.*)\)/)) {
			$name =~ s/^\s*($Ident).*/$1/;
		    ($line =~ /EXPORT_SYMBOL.*\((.*)\)/)) {
		if ($line =~ /^\+$Type\s*$Ident(?:\s+$Modifier)*\s*=\s*($zero_initializer)\s*;/ &&
		    !exclude_global_initialisers($realfile)) {
# check for unnecessary <signed> int declarations of short/long/long long
		while ($sline =~ m{\b($TypeMisordered(\s*\*)*|$C90_int_types)\b}g) {
			my $type = trim($1);
			next if ($type !~ /\bint\b/);
			next if ($type !~ /\b(?:short|long\s+long|long)\b/);
			my $new_type = $type;
			$new_type =~ s/\b\s*int\s*\b/ /;
			$new_type =~ s/\b\s*(?:un)?signed\b\s*/ /;
			$new_type =~ s/^const\s+//;
			$new_type = "unsigned $new_type" if ($type =~ /\bunsigned\b/);
			$new_type = "const $new_type" if ($type =~ /^const\b/);
			$new_type =~ s/\s+/ /g;
			$new_type = trim($new_type);
			if (WARN("UNNECESSARY_INT",
				 "Prefer '$new_type' over '$type' as the int is unnecessary\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~ s/\b\Q$type\E\b/$new_type/;
			}
		}

		}

# check for initialized const char arrays that should be static const
		if ($line =~ /^\+\s*const\s+(char|unsigned\s+char|_*u8|(?:[us]_)?int8_t)\s+\w+\s*\[\s*(?:\w+\s*)?\]\s*=\s*"/) {
			if (WARN("STATIC_CONST_CHAR_ARRAY",
				 "const array should probably be static const\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~ s/(^.\s*)const\b/${1}static const/;
			}
		}
		}
# check for const static or static <non ptr type> const declarations
# prefer 'static const <foo>' over 'const static <foo>' and 'static <foo> const'
		if ($sline =~ /^\+\s*const\s+static\s+($Type)\b/ ||
		    $sline =~ /^\+\s*static\s+($BasicType)\s+const\b/) {
			if (WARN("STATIC_CONST",
				 "Move const after static - use 'static const $1'\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~ s/\bconst\s+static\b/static const/;
				$fixed[$fixlinenr] =~ s/\bstatic\s+($BasicType)\s+const\b/static const $1/;
			}
		}

		}
		if ($line =~ /(\b$Type\s*$Ident)\s*\(\s*\)/) {
# prefer variants of (subsystem|netdev|dev|pr)_<level> to printk(KERN_<LEVEL>
		if ($line =~ /\b(printk(_once|_ratelimited)?)\s*\(\s*KERN_([A-Z]+)/) {
			my $printk = $1;
			my $modifier = $2;
			my $orig = $3;
			$modifier = "" if (!defined($modifier));
			$level .= $modifier;
			$level2 .= $modifier;
			     "Prefer [subsystem eg: netdev]_$level2([subsystem]dev, ... then dev_$level2(dev, ... then pr_$level(...  to $printk(KERN_$orig ...\n" . $herecurr);
# prefer dev_<level> to dev_printk(KERN_<LEVEL>
# trace_printk should not be used in production code.
		if ($line =~ /\b(trace_printk|trace_puts|ftrace_vprintk)\s*\(/) {
			WARN("TRACE_PRINTK",
			     "Do not use $1() in production code (this can be ignored if built only with a debug config option)\n" . $herecurr);
		}

# ENOTSUPP is not a standard error code and should be avoided in new patches.
# Folks usually mean EOPNOTSUPP (also called ENOTSUP), when they type ENOTSUPP.
# Similarly to ENOSYS warning a small number of false positives is expected.
		if (!$file && $line =~ /\bENOTSUPP\b/) {
			if (WARN("ENOTSUPP",
				 "ENOTSUPP is not a SUSV4 error code, prefer EOPNOTSUPP\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~ s/\bENOTSUPP\b/EOPNOTSUPP/;
			}
		}

		if ($perl_version_ok &&
		    $sline =~ /$Type\s*$Ident\s*$balanced_parens\s*\{/ &&
		    $sline !~ /\#\s*define\b.*do\s*\{/ &&
		    $sline !~ /}/) {
				  "open brace '{' following function definitions go on the next line\n" . $herecurr) &&
				$fixed_line =~ /(^..*$Type\s*$Ident\(.*\)\s*)\{(.*)$/;
			    $prefix !~ /[{,:]\s+$/) {
						# FRR: ignore , spacing in macros
					if ($ctx =~ /Wx./ and $realfile !~ m@.*\.lds\.h$@) {
						$ok = 1;
## 			# falsely report the parameters of functions.
		    $line =~ /\b(?:else|do)\{/) {
				$fixed[$fixlinenr] =~ s/^(\+.*(?:do|else|\)))\{/$1 {/;
		if ($line =~ /}(?!(?:,|;|\)|\}))\S/) {
# when !drivers/staging or command-line uses --strict
		if (($realfile !~ m@^(?:drivers/staging/)@ || $check_orig) &&
		    $perl_version_ok && defined($stat) &&
# check that goto labels aren't indented (allow a single space indentation)
# and ignore bitfield definitions like foo:1
# Strictly, labels can have whitespace after the identifier and before the :
# but this is not allowed here as many ?: uses would appear to be labels
                # FRR: has indented labels
		if (!$frr &&
                    $sline =~ /^.\s+[A-Za-z_][A-Za-z\d_]*:(?!\s*\d+)/ &&
		    $sline !~ /^. [A-Za-z\d_][A-Za-z\d_]*:/ &&
		    $sline !~ /^.\s+default:/) {
			if (WARN("INDENTED_LABEL",
				 "labels should not be indented\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~
				    s/^(.)\s+/$1/;
			}
		}

# check if a statement with a comma should be two statements like:
#	foo = bar(),	/* comma should be semicolon */
#	bar = baz();
		if (defined($stat) &&
		    $stat =~ /^\+\s*(?:$Lval\s*$Assignment\s*)?$FuncArg\s*,\s*(?:$Lval\s*$Assignment\s*)?$FuncArg\s*;\s*$/) {
			my $cnt = statement_rawlines($stat);
			my $herectx = get_stat_here($linenr, $cnt, $here);
			WARN("SUSPECT_COMMA_SEMICOLON",
			     "Possible comma where semicolon could be used\n" . $herectx);
		}

			if ($perl_version_ok &&
		}
		if ($perl_version_ok &&
		if ($perl_version_ok &&
			if ($name ne 'EOF' && $name ne 'ERROR' && $name !~ /^EPOLL/) {
				if (ERROR("ASSIGN_IN_IF",
					  "do not use assignment in if condition\n" . $herecurr) &&
				    $fix && $perl_version_ok) {
					if ($rawline =~ /^\+(\s+)if\s*\(\s*(\!)?\s*\(\s*(($Lval)\s*=\s*$LvalOrFunc)\s*\)\s*(?:($Compare)\s*($FuncArg))?\s*\)\s*(\{)?\s*$/) {
						my $space = $1;
						my $not = $2;
						my $statement = $3;
						my $assigned = $4;
						my $test = $8;
						my $against = $9;
						my $brace = $15;
						fix_delete_line($fixlinenr, $rawline);
						fix_insert_line($fixlinenr, "$space$statement;");
						my $newline = "${space}if (";
						$newline .= '!' if defined($not);
						$newline .= '(' if (defined $not && defined($test) && defined($against));
						$newline .= "$assigned";
						$newline .= " $test $against" if (defined($test) && defined($against));
						$newline .= ')' if (defined $not && defined($test) && defined($against));
						$newline .= ')';
						$newline .= " {" if (defined($brace));
						fix_insert_line($fixlinenr + 1, $newline);
					}
				}
			$s =~ s/$;//g;	# Remove any comments
			$s =~ s/$;//g;	# Remove any comments
#Ignore some autogenerated defines and enum values
			    $var !~ /^(?:[A-Z]+_){1,5}[A-Z]{1,3}[a-z]/ &&
#Ignore SI style variants like nS, mV and dB
#(ie: max_uV, regulator_min_uA_show, RANGE_mA_VALUE)
			    $var !~ /^(?:[a-z0-9_]*|[A-Z0-9_]*)?_?[a-z][A-Z](?:_[a-z0-9_]+|_[A-Z0-9_]+)?$/ &&
				$define_args =~ s/\\\+?//g;
			while ($dstat =~ s/\([^\(\)]*\)/1u/ ||
			       $dstat =~ s/\{[^\{\}]*\}/1u/ ||
			       $dstat =~ s/.\[[^\[\]]*\]/1u/)
			# Flatten any obvious string concatenation.
			my $herectx = get_stat_here($linenr, $stmt_cnt, $here);
			    $dstat !~ /^while\s*$Constant\s*$Constant\s*$/ &&		# while (...) {...}
				$tmp_stmt =~ s/\b(__must_be_array|offsetof|sizeof|sizeof_field|__stringify|typeof|__typeof__|__builtin\w+|typecheck\s*\(\s*$Type\s*,|\#+)\s*\(*\s*$arg\s*\)*\b//g;
				my $use_cnt = () = $tmp_stmt =~ /\b$arg\b/g;
				my $herectx = get_stat_here($linenr, $cnt, $here);
		if ($perl_version_ok &&
				my $herectx = get_stat_here($linenr, $cnt, $here);
				my $herectx = get_stat_here($linenr, $cnt, $here);
				my $herectx = get_stat_here($linenr, $cnt, $here);
		    $line =~ /\b(frr_with_)/) {
		    my ($level, $endln, @chunks) =
			ctx_statement_full($linenr, $realcnt, $-[0]);

		    # Check the condition.
		    my ($cond, $block) = @{$chunks[0]};
		    #print "CHECKING<$linenr> cond<$cond> block<$block>\n";
		    if (defined $cond) {
			substr($block, 0, length($cond), '');
		    }

		    if ($level == 0 && $block !~ /^\s*\{/) {
			my $herectx = $here . "\n";
			my $cnt = statement_rawlines($block);
			for (my $n = 0; $n < $cnt; $n++) {
			    $herectx .= raw_line($linenr, $n) . "\n";
			WARN("BRACES",
			     "braces {} are mandatory for frr_with_* blocks\n" . $herectx);
		    }
# check for unnecessary function tracing like uses
# This does not use $logFunctions because there are many instances like
# 'dprintk(FOO, "%s()\n", __func__);' which do not match $logFunctions
		if ($rawline =~ /^\+.*\([^"]*"$tracing_logging_tags{0,3}%s(?:\s*\(\s*\)\s*)?$tracing_logging_tags{0,3}(?:\\n)?"\s*,\s*__func__\s*\)\s*;/) {
			if (WARN("TRACING_LOGGING",
				 "Unnecessary ftrace-like logging - prefer using ftrace\n" . $herecurr) &&
			    $fix) {
                                fix_delete_line($fixlinenr, $rawline);
			}
		}

		if ($line =~ /$String[A-Z_]/ ||
		    ($line =~ /([A-Za-z0-9_]+)$String/ && $1 !~ /^[Lu]$/)) {
			if (CHK("CONCATENATED_STRING",
				"Concatenated strings should use spaces between elements\n" . $herecurr) &&
			    $fix) {
				while ($line =~ /($String)/g) {
					my $extracted_string = substr($rawline, $-[0], $+[0] - $-[0]);
					$fixed[$fixlinenr] =~ s/\Q$extracted_string\E([A-Za-z0-9_])/$extracted_string $1/;
					$fixed[$fixlinenr] =~ s/([A-Za-z0-9_])\Q$extracted_string\E/$1 $extracted_string/;
				}
			}
		if ($line =~ /$String\s*[Lu]?"/) {
			# FRR: WARN -> CHK
			if (CHK("STRING_FRAGMENTS",
				 "Consecutive strings are generally better as a single string\n" . $herecurr) &&
			    $fix) {
				while ($line =~ /($String)(?=\s*")/g) {
					my $extracted_string = substr($rawline, $-[0], $+[0] - $-[0]);
					$fixed[$fixlinenr] =~ s/\Q$extracted_string\E\s*"/substr($extracted_string, 0, -1)/e;
				}
			}
		if ($rawline =~ /\\$/ && $sline =~ tr/"/"/ % 2) {
			WARN("IF_0",
			     "Consider removing the code enclosed by this #if 0 and its #endif\n" . $herecurr);
		}

# warn about #if 1
		if ($line =~ /^.\s*\#\s*if\s+1\b/) {
			WARN("IF_1",
			     "Consider removing the #if 1 and its #endif\n" . $herecurr);
			if ($s =~ /(?:^|\n)[ \+]\s*(?:$Type\s*)?\Q$testval\E\s*=\s*(?:\([^\)]*\)\s*)?\s*$allocFunctions\s*\(/ &&
			    $s !~ /\b__GFP_NOWARN\b/ ) {
# check for unnecessary use of %h[xudi] and %hh[xudi] in logging functions
		if (defined $stat &&
		    $line =~ /\b$logFunctions\s*\(/ &&
		    index($stat, '"') >= 0) {
			my $lc = $stat =~ tr@\n@@;
			$lc = $lc + $linenr;
			my $stat_real = get_stat_real($linenr, $lc);
			pos($stat_real) = index($stat_real, '"');
			while ($stat_real =~ /[^\"%]*(%[\#\d\.\*\-]*(h+)[idux])/g) {
				my $pspec = $1;
				my $h = $2;
				my $lineoff = substr($stat_real, 0, $-[1]) =~ tr@\n@@;
				if (WARN("UNNECESSARY_MODIFIER",
					 "Integer promotion: Using '$h' in '$pspec' is unnecessary\n" . "$here\n$stat_real\n") &&
				    $fix && $fixed[$fixlinenr + $lineoff] =~ /^\+/) {
					my $nspec = $pspec;
					$nspec =~ s/h//g;
					$fixed[$fixlinenr + $lineoff] =~ s/\Q$pspec\E/$nspec/;
				}
			}
		}

		if ($perl_version_ok &&
		if ($perl_version_ok) {
				    "usleep_range is preferred over udelay; see Documentation/timers/timers-howto.rst\n" . $herecurr);
				     "msleep < 20ms can sleep for up to 20ms; see Documentation/timers/timers-howto.rst\n" . $herecurr);
			wmb
# check for data_race without a comment.
		if ($line =~ /\bdata_race\s*\(/) {
			if (!ctx_has_comment($first_line, $linenr)) {
				WARN("DATA_RACE",
				     "data_race without comment\n" . $herecurr);
			}
		}

# Check for compiler attributes
		# FRR: doesn't use kernel macro replacements for compiler attributes.
		if (!$frr &&
		    $realfile !~ m@\binclude/uapi/@ &&
		    $rawline =~ /\b__attribute__\s*\(\s*($balanced_parens)\s*\)/) {
			my $attr = $1;
			$attr =~ s/\s*\(\s*(.*)\)\s*/$1/;

			my %attr_list = (
				"alias"				=> "__alias",
				"aligned"			=> "__aligned",
				"always_inline"			=> "__always_inline",
				"assume_aligned"		=> "__assume_aligned",
				"cold"				=> "__cold",
				"const"				=> "__attribute_const__",
				"copy"				=> "__copy",
				"designated_init"		=> "__designated_init",
				"externally_visible"		=> "__visible",
				"format"			=> "printf|scanf",
				"gnu_inline"			=> "__gnu_inline",
				"malloc"			=> "__malloc",
				"mode"				=> "__mode",
				"no_caller_saved_registers"	=> "__no_caller_saved_registers",
				"noclone"			=> "__noclone",
				"noinline"			=> "noinline",
				"nonstring"			=> "__nonstring",
				"noreturn"			=> "__noreturn",
				"packed"			=> "__packed",
				"pure"				=> "__pure",
				"section"			=> "__section",
				"used"				=> "__used",
				"weak"				=> "__weak"
			);

			while ($attr =~ /\s*(\w+)\s*(${balanced_parens})?/g) {
				my $orig_attr = $1;
				my $params = '';
				$params = $2 if defined($2);
				my $curr_attr = $orig_attr;
				$curr_attr =~ s/^[\s_]+|[\s_]+$//g;
				if (exists($attr_list{$curr_attr})) {
					my $new = $attr_list{$curr_attr};
					if ($curr_attr eq "format" && $params) {
						$params =~ /^\s*\(\s*(\w+)\s*,\s*(.*)/;
						$new = "__$1\($2";
					} else {
						$new = "$new$params";
					}
					if (WARN("PREFER_DEFINED_ATTRIBUTE_MACRO",
						 "Prefer $new over __attribute__(($orig_attr$params))\n" . $herecurr) &&
					    $fix) {
						my $remove = "\Q$orig_attr\E" . '\s*' . "\Q$params\E" . '(?:\s*,\s*)?';
						$fixed[$fixlinenr] =~ s/$remove//;
						$fixed[$fixlinenr] =~ s/\b__attribute__/$new __attribute__/;
						$fixed[$fixlinenr] =~ s/\}\Q$new\E/} $new/;
						$fixed[$fixlinenr] =~ s/ __attribute__\s*\(\s*\(\s*\)\s*\)//;
					}
				}
			}
			# Check for __attribute__ unused, prefer __always_unused or __maybe_unused
			if ($attr =~ /^_*unused/) {
				WARN("PREFER_DEFINED_ATTRIBUTE_MACRO",
				     "__always_unused or __maybe_unused is preferred over __attribute__((__unused__))\n" . $herecurr);
			}
		}
		if (!$frr &&
		    $perl_version_ok &&
		    $line =~ /(?:$Declare|$DeclareMisordered)\s*$Ident\s*$balanced_parens\s*(?:$Attribute)?\s*;/ &&
		    ($line =~ /\b__attribute__\s*\(\s*\(.*\bweak\b/ ||
		     $line =~ /\b__weak\b/)) {
			ERROR("WEAK_DECLARATION",
			      "Using weak declarations can have unintended link defects\n" . $herecurr);
		}
		if (!$frr &&
		    $realfile !~ m@\binclude/uapi/@ &&
			my $suffix = "";
			my $newconst = $const;
			$newconst =~ s/${Int_type}$//;
			$suffix .= 'U' if ($cast =~ /\bunsigned\b/);
			if ($cast =~ /\blong\s+long\b/) {
			    $suffix .= 'LL';
			} elsif ($cast =~ /\blong\b/) {
			    $suffix .= 'L';
			}
				 "Unnecessary typecast of c90 int constant - '$cast$const' could be '$const$suffix'\n" . $herecurr) &&
# check for vsprintf extension %p<foo> misuses
		if (!$frr &&
		    $perl_version_ok &&
			my $stat_real;

				my $specifier;
				my $extension;
				my $qualifier;
				my $bad_specifier = "";

				while ($fmt =~ /(\%[\*\d\.]*p(\w)(\w*))/g) {
					$specifier = $1;
					$extension = $2;
					$qualifier = $3;
					if ($extension !~ /[4SsBKRraEehMmIiUDdgVCbGNOxtf]/ ||
					    ($extension eq "f" &&
					     defined $qualifier && $qualifier !~ /^w/) ||
					    ($extension eq "4" &&
					     defined $qualifier && $qualifier !~ /^cc/)) {
						$bad_specifier = $specifier;
						last;
					}
					if ($extension eq "x" && !defined($stat_real)) {
						if (!defined($stat_real)) {
							$stat_real = get_stat_real($linenr, $lc);
						}
						WARN("VSPRINTF_SPECIFIER_PX",
						     "Using vsprintf specifier '\%px' potentially exposes the kernel memory layout, if you don't really need the address please consider using '\%p'.\n" . "$here\n$stat_real\n");
					}
				if ($bad_specifier ne "") {
					my $stat_real = get_stat_real($linenr, $lc);
					my $ext_type = "Invalid";
					my $use = "";
					if ($bad_specifier =~ /p[Ff]/) {
						$use = " - use %pS instead";
						$use =~ s/pS/ps/ if ($bad_specifier =~ /pf/);
					}

					WARN("VSPRINTF_POINTER_EXTENSION",
					     "$ext_type vsprintf pointer extension '$bad_specifier'$use\n" . "$here\n$stat_real\n");
		if ($perl_version_ok &&
#		if ($perl_version_ok &&
#		if ($perl_version_ok &&
#		if ($perl_version_ok &&
# strlcpy uses that should likely be strscpy
		if (!$frr &&
                    $line =~ /\bstrlcpy\s*\(/) {
			WARN("STRLCPY",
			     "Prefer strscpy over strlcpy - see: https://lore.kernel.org/r/CAHk-=wgfRnXz0W3D37d01q3JFkr_i_uTL=V6A6G1oUZcprmknw\@mail.gmail.com/\n" . $herecurr);
		}

		if ($perl_version_ok &&
		if ($perl_version_ok &&
				     "usleep_range should not use min == max args; see Documentation/timers/timers-howto.rst\n" . "$here\n$stat\n");
				     "usleep_range args reversed, use min then max; see Documentation/timers/timers-howto.rst\n" . "$here\n$stat\n");
		if ($perl_version_ok &&
			my $stat_real = get_stat_real($linenr, $lc);
# check for simple sscanf that should be kstrto<foo>
		if (!$frr &&
		    $perl_version_ok &&
		    defined $stat &&
		    $line =~ /\bsscanf\b/) {
			my $lc = $stat =~ tr@\n@@;
			$lc = $lc + $linenr;
			my $stat_real = get_stat_real($linenr, $lc);
			if ($stat_real =~ /\bsscanf\b\s*\(\s*$FuncArg\s*,\s*("[^"]+")/) {
				my $format = $6;
				my $count = $format =~ tr@%@%@;
				if ($count == 1 &&
				    $format =~ /^"\%(?i:ll[udxi]|[udxi]ll|ll|[hl]h?[udxi]|[udxi][hl]h?|[hl]h?|[udxi])"$/) {
					WARN("SSCANF_TO_KSTRTO",
					     "Prefer kstrto<type> to single variable sscanf\n" . "$here\n$stat_real\n");
				}
			}
		}

			if ($s =~ /^\s*;/)
		if ($perl_version_ok &&
				    "__setup appears un-documented -- check Documentation/admin-guide/kernel-parameters.txt\n" . $herecurr);
# check for pointless casting of alloc functions
		if ($line =~ /\*\s*\)\s*$allocFunctions\b/) {
		if ($perl_version_ok &&
		    $line =~ /\b($Lval)\s*\=\s*(?:$balanced_parens)?\s*((?:kv|k|v)[mz]alloc(?:_node)?)\s*\(\s*(sizeof\s*\(\s*struct\s+$Lval\s*\))/) {
		if ($perl_version_ok &&
				my $herectx = get_stat_here($linenr, $cnt, $here);

		if ($perl_version_ok &&
		    $line =~ /\b($Lval)\s*\=\s*(?:$balanced_parens)?\s*krealloc\s*\(\s*($Lval)\s*,/ &&
		    $1 eq $3) {
		if ($line =~ /\b((?:devm_)?(?:kcalloc|kmalloc_array))\s*\(\s*sizeof\b/) {
# check for IS_ENABLED() without CONFIG_<FOO> ($rawline for comments too)
		if ($rawline =~ /\bIS_ENABLED\s*\(\s*(\w+)\s*\)/ && $1 !~ /^${CONFIG_}/) {
			WARN("IS_ENABLED_CONFIG",
			     "IS_ENABLED($1) is normally used as IS_ENABLED(${CONFIG_}$1)\n" . $herecurr);
		}

		if ($line =~ /^\+\s*#\s*if\s+defined(?:\s*\(?\s*|\s+)(${CONFIG_}[A-Z_]+)\s*\)?\s*\|\|\s*defined(?:\s*\(?\s*|\s+)\1_MODULE\s*\)?\s*$/) {
				 "Prefer IS_ENABLED(<FOO>) to ${CONFIG_}<FOO> || ${CONFIG_}<FOO>_MODULE\n" . $herecurr) &&
# check for /* fallthrough */ like comment, prefer fallthrough;
		my @fallthroughs = (
			'fallthrough',
			'@fallthrough@',
			'lint -fallthrough[ \t]*',
			'intentional(?:ly)?[ \t]*fall(?:(?:s | |-)[Tt]|t)hr(?:ough|u|ew)',
			'(?:else,?\s*)?FALL(?:S | |-)?THR(?:OUGH|U|EW)[ \t.!]*(?:-[^\n\r]*)?',
			'Fall(?:(?:s | |-)[Tt]|t)hr(?:ough|u|ew)[ \t.!]*(?:-[^\n\r]*)?',
			'fall(?:s | |-)?thr(?:ough|u|ew)[ \t.!]*(?:-[^\n\r]*)?',
		    );
		if ($raw_comment ne '') {
			foreach my $ft (@fallthroughs) {
				if ($raw_comment =~ /$ft/) {
					my $msg_level = \&WARN;
					$msg_level = \&CHK if ($file);
					&{$msg_level}("PREFER_FALLTHROUGH",
						      "Prefer 'fallthrough;' over fallthrough comment\n" . $herecurr);
					last;
				}
		if ($perl_version_ok &&
			my $herectx = get_stat_here($linenr, $cnt, $here);

# recommend kstrto* over simple_strto* and strict_strto*
		if (!$frr &&
		    $line =~ /\b((simple|strict)_(strto(l|ll|ul|ull)))\s*\(/) {
			WARN("CONSIDER_KSTRTO",
			     "$1 is obsolete, use k$3 instead\n" . $herecurr);
		}

# check for spin_is_locked(), suggest lockdep instead
		if ($line =~ /\bspin_is_locked\(/) {
			WARN("USE_LOCKDEP",
			     "Where possible, use lockdep_assert_held instead of assertions based on spin_is_locked\n" . $herecurr);
		}

# check for deprecated apis
		if ($line =~ /\b($deprecated_apis_search)\b\s*\(/) {
			my $deprecated_api = $1;
			my $new_api = $deprecated_apis{$deprecated_api};
			WARN("DEPRECATED_API",
			     "Deprecated use of '$deprecated_api', prefer '$new_api' instead\n" . $herecurr);
		}

# check for various structs that are normally const (ops, kgdb, device_tree)
# and avoid what seem like struct definitions 'struct foo {'
		if (!$frr &&
		    defined($const_structs) &&
		    $line !~ /\bconst\b/ &&
		    $line =~ /\bstruct\s+($const_structs)\b(?!\s*\{)/) {
			WARN("CONST_STRUCT",
			     "struct $1 should normally be const\n" . $herecurr);
		}

# ignore designated initializers using NR_CPUS
		    $line !~ /\[[^\]]*NR_CPUS[^\]]*\.\.\.[^\]]*\]/ &&
		    $line !~ /^.\s*\.\w+\s*=\s*.*\bNR_CPUS\b/)
		if ($perl_version_ok &&
# return sysfs_emit(foo, fmt, ...) fmt without newline
		if ($line =~ /\breturn\s+sysfs_emit\s*\(\s*$FuncArg\s*,\s*($String)/ &&
		    substr($rawline, $-[6], $+[6] - $-[6]) !~ /\\n"$/) {
			my $offset = $+[6] - 1;
			if (WARN("SYSFS_EMIT",
				 "return sysfs_emit(...) formats should include a terminating newline\n" . $herecurr) &&
			    $fix) {
				substr($fixed[$fixlinenr], $offset, 0) = '\\n';
			}
		}

# nested likely/unlikely calls
		if ($line =~ /\b(?:(?:un)?likely)\s*\(\s*!?\s*(IS_ERR(?:_OR_NULL|_VALUE)?|WARN)/) {
			WARN("LIKELY_MISUSE",
			     "nested (un)?likely() calls, $1 already uses unlikely() internally\n" . $herecurr);
		}

# check for DEVICE_ATTR uses that could be DEVICE_ATTR_<FOO>
# and whether or not function naming is typical and if
# DEVICE_ATTR permissions uses are unusual too
		if ($perl_version_ok &&
		    defined $stat &&
		    $stat =~ /\bDEVICE_ATTR\s*\(\s*(\w+)\s*,\s*\(?\s*(\s*(?:${multi_mode_perms_string_search}|0[0-7]{3,3})\s*)\s*\)?\s*,\s*(\w+)\s*,\s*(\w+)\s*\)/) {
			my $var = $1;
			my $perms = $2;
			my $show = $3;
			my $store = $4;
			my $octal_perms = perms_to_octal($perms);
			if ($show =~ /^${var}_show$/ &&
			    $store =~ /^${var}_store$/ &&
			    $octal_perms eq "0644") {
				if (WARN("DEVICE_ATTR_RW",
					 "Use DEVICE_ATTR_RW\n" . $herecurr) &&
				    $fix) {
					$fixed[$fixlinenr] =~ s/\bDEVICE_ATTR\s*\(\s*$var\s*,\s*\Q$perms\E\s*,\s*$show\s*,\s*$store\s*\)/DEVICE_ATTR_RW(${var})/;
				}
			} elsif ($show =~ /^${var}_show$/ &&
				 $store =~ /^NULL$/ &&
				 $octal_perms eq "0444") {
				if (WARN("DEVICE_ATTR_RO",
					 "Use DEVICE_ATTR_RO\n" . $herecurr) &&
				    $fix) {
					$fixed[$fixlinenr] =~ s/\bDEVICE_ATTR\s*\(\s*$var\s*,\s*\Q$perms\E\s*,\s*$show\s*,\s*NULL\s*\)/DEVICE_ATTR_RO(${var})/;
				}
			} elsif ($show =~ /^NULL$/ &&
				 $store =~ /^${var}_store$/ &&
				 $octal_perms eq "0200") {
				if (WARN("DEVICE_ATTR_WO",
					 "Use DEVICE_ATTR_WO\n" . $herecurr) &&
				    $fix) {
					$fixed[$fixlinenr] =~ s/\bDEVICE_ATTR\s*\(\s*$var\s*,\s*\Q$perms\E\s*,\s*NULL\s*,\s*$store\s*\)/DEVICE_ATTR_WO(${var})/;
				}
			} elsif ($octal_perms eq "0644" ||
				 $octal_perms eq "0444" ||
				 $octal_perms eq "0200") {
				my $newshow = "$show";
				$newshow = "${var}_show" if ($show ne "NULL" && $show ne "${var}_show");
				my $newstore = $store;
				$newstore = "${var}_store" if ($store ne "NULL" && $store ne "${var}_store");
				my $rename = "";
				if ($show ne $newshow) {
					$rename .= " '$show' to '$newshow'";
				}
				if ($store ne $newstore) {
					$rename .= " '$store' to '$newstore'";
				}
				WARN("DEVICE_ATTR_FUNCTIONS",
				     "Consider renaming function(s)$rename\n" . $herecurr);
			} else {
				WARN("DEVICE_ATTR_PERMS",
				     "DEVICE_ATTR unusual permissions '$perms' used\n" . $herecurr);
			}
		}

# o Ignore module_param*(...) uses with a decimal 0 permission as that has a
#   specific definition of not visible in sysfs.
# o Ignore proc_create*(...) uses with a decimal 0 permission as that means
#   use the default permissions
		if ($perl_version_ok &&
				my $stat_real = get_stat_real($linenr, $lc);
					if (!($func =~ /^(?:module_param|proc_create)/ && $val eq "0") &&
					    (($val =~ /^$Int$/ && $val !~ /^$Octal$/) ||
					     ($val =~ /^$Octal$/ && length($val) ne 4))) {
		while ($line =~ m{\b($multi_mode_perms_string_search)\b}g) {
			my $oval = $1;
			my $octal = perms_to_octal($oval);
				$fixed[$fixlinenr] =~ s/\Q$oval\E/$octal/;
# check for sysctl duplicate constants
		if ($line =~ /\.extra[12]\s*=\s*&(zero|one|int_max)\b/) {
			WARN("DUPLICATED_SYSCTL_CONST",
				"duplicated sysctl range checking value '$1', consider using the shared one in include/linux/sysctl.h\n" . $herecurr);
		}

	# This is not a patch, and we are in 'no-patch' mode so
	if ($is_patch && $has_commit_log && $chk_signoff) {
		if ($signoff == 0) {
			ERROR("MISSING_SIGN_OFF",
			      "Missing Signed-off-by: line(s)\n");
		} elsif ($authorsignoff != 1) {
			# authorsignoff values:
			# 0 -> missing sign off
			# 1 -> sign off identical
			# 2 -> names and addresses match, comments mismatch
			# 3 -> addresses match, names different
			# 4 -> names match, addresses different
			# 5 -> names match, addresses excluding subaddress details (refer RFC 5233) match

			my $sob_msg = "'From: $author' != 'Signed-off-by: $author_sob'";

			if ($authorsignoff == 0) {
				ERROR("NO_AUTHOR_SIGN_OFF",
				      "Missing Signed-off-by: line by nominal patch author '$author'\n");
			} elsif ($authorsignoff == 2) {
				CHK("FROM_SIGN_OFF_MISMATCH",
				    "From:/Signed-off-by: email comments mismatch: $sob_msg\n");
			} elsif ($authorsignoff == 3) {
				WARN("FROM_SIGN_OFF_MISMATCH",
				     "From:/Signed-off-by: email name mismatch: $sob_msg\n");
			} elsif ($authorsignoff == 4) {
				WARN("FROM_SIGN_OFF_MISMATCH",
				     "From:/Signed-off-by: email address mismatch: $sob_msg\n");
			} elsif ($authorsignoff == 5) {
				WARN("FROM_SIGN_OFF_MISMATCH",
				     "From:/Signed-off-by: email subaddress mismatch: $sob_msg\n");
			}
		}