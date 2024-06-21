
/*** MODULEINFO
    <depend>pjproject</depend>
	<depend>res_pjsip</depend>
	<depend>res_pjsip_session</depend>
	<support_level>core</support_level>
 ***/

#include "asterisk.h"
#include "asterisk/pbx.h"
#include "asterisk/module.h"
#include "asterisk/app.h"
#include "asterisk/channel.h"
#include "asterisk/bridge.h"
#include "asterisk/features_config.h"
#include "asterisk/res_pjsip.h"

/*** DOCUMENTATION
	<application name="WaitUntilPrecondition" language="en_US">
		<synopsis>
			Wait (sleep) until precondition is finished.
		</synopsis>
		<syntax>
			<parameter name="period_ms" required="true" />
			<parameter name="timeout_ms" required="true" />
		</syntax>
		<description>
			<para>Waits until precondition.</para>
			<para>Sets <variable>PRECONDITION_STATUS</variable> to one of the following values:</para>
			<variablelist>
				<variable name="PRECONDITION_STATUS">
					<value name="SUCCESS">
						Wait for precondition succeeded.
					</value>
					<value name="FAILURE">
						Wait for precondition failed.
					</value>
				</variable>
			</variablelist>
		</description>
	</application>
 ***/

static char *app = "WaitForPrecondition";
pj_bool_t ast_get_precondition_complete(struct ast_channel *ast);

enum mt_precodition_state {
	MT_PRECONDITION_STATE_NOT_OCCURED,
	MT_PRECONDITION_STATE_IN_PROGRESS,
	MT_PRECONDITION_STATE_COMPLETED
};

static int waituntilprecondition_exec(struct ast_channel *chan, const char* data)
{
	pj_bool_t precondition_status;
	char *parse;
	int period_ms = 200;
	unsigned int timeout_ms = 300000U;
	int elapsed_time = 0;

	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(period_ms);
		AST_APP_ARG(timeout_ms);
	);

	parse = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, parse);

	if (!ast_strlen_zero(args.period_ms)) {
		if (sscanf(args.period_ms, "%30d", &period_ms) != 1 || period_ms < 0) {
			ast_log(LOG_ERROR, "Argument period_ms required' must be an integer greater than or equal to zero.\n");
			pbx_builtin_setvar_helper(chan, "PRECONDITION_STATUS", "FAILURE");
			return -1;
		}
	}

	if (!ast_strlen_zero(args.timeout_ms)) {
		if (sscanf(args.timeout_ms, "%30d", &timeout_ms) != 1 || timeout_ms < 0) {
			ast_log(LOG_ERROR, "Argument timeout_ms required' must be an integer greater than or equal to zero.\n");
			pbx_builtin_setvar_helper(chan, "PRECONDITION_STATUS", "FAILURE");
			return -1;
		}
	}

	do {
		precondition_status = ast_get_precondition_complete(chan);
		if (precondition_status) {
			ast_log(LOG_NOTICE, "precondition status: success %d", precondition_status);
			pbx_builtin_setvar_helper(chan, "PRECONDITION_STATUS", "SUCCESS");
			break;
		}
		if (elapsed_time >= timeout_ms) {
			pbx_builtin_setvar_helper(chan, "PRECONDITION_STATUS", "FAILURE");
			break;
		}
		ast_safe_sleep(chan, period_ms);
		elapsed_time += period_ms;
	}
	while (1);

	return 0;
}

static int unload_module(void)
{
	return ast_unregister_application(app);
}

static int load_module(void)
{
	return ast_register_application_xml(app, waituntilprecondition_exec);
}

AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, "Wait until precondition");
